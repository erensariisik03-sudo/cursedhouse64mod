#include "chat_bot_http.h"
#include "voice_ai_bridge.h"
#include <cctype>
#if defined(__ANDROID__)
#include "voice_ai_core.h"
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

// HTTPS, Android'in kendi java.net.HttpURLConnection sınıfıyla JNI üzerinden yapılır:
// native TLS/sertifika yığını eklemek gerekmez. Yalnız arka plan thread'inde çalışır;
// Unity/IL2CPP nesnelerine dokunmaz. Gövde, anahtar ve yanıt loglanmaz.
namespace chat_bot_http {
namespace {
std::atomic<JavaVM*> g_vm{nullptr};
JavaVM* FindVM() {
    if (auto* vm = g_vm.load()) return vm;
    using Get = jint (*)(JavaVM**, jsize, jsize*);
    for (const char* lib : {static_cast<const char*>(nullptr), "libnativehelper.so", "libart.so"}) {
        void* handle = lib ? dlopen(lib, RTLD_NOW | RTLD_NOLOAD) : RTLD_DEFAULT;
        if (!handle) continue;
        auto get = reinterpret_cast<Get>(dlsym(handle, "JNI_GetCreatedJavaVMs"));
        JavaVM* vm = nullptr; jsize n = 0;
        if (get && get(&vm, 1, &n) == JNI_OK && n > 0 && vm) { g_vm = vm; return vm; }
    }
    return nullptr;
}
// Android NDK AttachCurrentThread(JNIEnv**, void*) ister; masaüstü JDK başlığı void** ister.
template<class V> auto AttachImpl(V* vm, JNIEnv** env, int) -> decltype(vm->AttachCurrentThread(env, nullptr)) { return vm->AttachCurrentThread(env, nullptr); }
template<class V> jint AttachImpl(V* vm, JNIEnv** env, long) { return vm->AttachCurrentThread(reinterpret_cast<void**>(env), nullptr); }
struct Attach {
    JavaVM* vm; JNIEnv* env = nullptr; bool attached = false;
    explicit Attach(JavaVM* v) : vm(v) {
        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return;
        if (AttachImpl(vm, &env, 0) == JNI_OK) attached = true; else env = nullptr;
    }
    ~Attach() { if (attached) vm->DetachCurrentThread(); }
};
bool Bad(JNIEnv* env) { if (env->ExceptionCheck()) { env->ExceptionClear(); return true; } return false; }
}
void SetVM(JavaVM* vm) { g_vm = vm; }

namespace {
std::u16string Utf8ToUtf16(const std::string& in) {
    std::u16string out;
    for (size_t i = 0; i < in.size();) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        uint32_t cp = 0xFFFD; size_t len = 1;
        if (c < 0x80) cp = c;
        else if (c >= 0xC2 && c < 0xE0 && i + 1 < in.size() && (in[i+1] & 0xC0) == 0x80) { cp = ((c & 0x1F) << 6) | (in[i+1] & 0x3F); len = 2; }
        else if (c >= 0xE0 && c < 0xF0 && i + 2 < in.size() && (in[i+1] & 0xC0) == 0x80 && (in[i+2] & 0xC0) == 0x80) { cp = ((c & 0x0F) << 12) | ((in[i+1] & 0x3F) << 6) | (in[i+2] & 0x3F); len = 3; if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD; }
        else if (c >= 0xF0 && c < 0xF5 && i + 3 < in.size() && (in[i+1] & 0xC0) == 0x80 && (in[i+2] & 0xC0) == 0x80 && (in[i+3] & 0xC0) == 0x80) { cp = ((c & 0x07) << 18) | ((in[i+1] & 0x3F) << 12) | ((in[i+2] & 0x3F) << 6) | (in[i+3] & 0x3F); len = 4; if (cp < 0x10000 || cp > 0x10FFFF) cp = 0xFFFD; }
        i += len;
        if (cp >= 0x10000) { cp -= 0x10000; out += static_cast<char16_t>(0xD800 + (cp >> 10)); out += static_cast<char16_t>(0xDC00 + (cp & 0x3FF)); }
        else out += static_cast<char16_t>(cp);
    }
    return out;
}
}
bool WriteClipboard(const std::string& text) {
    if (text.empty() || text.size() > 8192) return false;
    JavaVM* vm = FindVM();
    if (!vm) return false;
    Attach thread(vm);
    JNIEnv* env = thread.env;
    if (!env || env->PushLocalFrame(24) != 0) return false;
    struct Frame { JNIEnv* e; ~Frame() { e->PopLocalFrame(nullptr); } } frame{env};
    jclass at = env->FindClass("android/app/ActivityThread");
    jclass ctx_cls = env->FindClass("android/content/Context");
    jclass cm_cls = env->FindClass("android/content/ClipboardManager");
    jclass cd_cls = env->FindClass("android/content/ClipData");
    if (Bad(env) || !at || !ctx_cls || !cm_cls || !cd_cls) return false;
    jmethodID current = env->GetStaticMethodID(at, "currentApplication", "()Landroid/app/Application;");
    jmethodID service = env->GetMethodID(ctx_cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jmethodID set_clip = env->GetMethodID(cm_cls, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    jmethodID make = env->GetStaticMethodID(cd_cls, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    if (Bad(env) || !current || !service || !set_clip || !make) return false;
    jobject app = env->CallStaticObjectMethod(at, current);
    if (Bad(env) || !app) return false;
    jobject manager = env->CallObjectMethod(app, service, env->NewStringUTF("clipboard"));
    if (Bad(env) || !manager) return false;
    const std::u16string wide = Utf8ToUtf16(text);
    jstring label = env->NewStringUTF("multiplayermod");
    jstring body = env->NewString(reinterpret_cast<const jchar*>(wide.data()), static_cast<jsize>(wide.size()));
    if (Bad(env) || !label || !body) return false;
    jobject clip = env->CallStaticObjectMethod(cd_cls, make, label, body);
    if (Bad(env) || !clip) return false;
    env->CallVoidMethod(manager, set_clip, clip);
    return !Bad(env);
}

bool ReadClipboard(std::string& out) {
    out.clear();
    JavaVM* vm = FindVM();
    if (!vm) return false;
    Attach thread(vm);
    JNIEnv* env = thread.env;
    if (!env || env->PushLocalFrame(24) != 0) return false;
    struct Frame { JNIEnv* e; ~Frame() { e->PopLocalFrame(nullptr); } } frame{env};
    jclass at = env->FindClass("android/app/ActivityThread");
    jclass ctx_cls = env->FindClass("android/content/Context");
    jclass cm_cls = env->FindClass("android/content/ClipboardManager");
    jclass cd_cls = env->FindClass("android/content/ClipData");
    jclass item_cls = env->FindClass("android/content/ClipData$Item");
    jclass cs_cls = env->FindClass("java/lang/CharSequence");
    if (Bad(env) || !at || !ctx_cls || !cm_cls || !cd_cls || !item_cls || !cs_cls) return false;
    jmethodID current = env->GetStaticMethodID(at, "currentApplication", "()Landroid/app/Application;");
    jmethodID service = env->GetMethodID(ctx_cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jmethodID primary = env->GetMethodID(cm_cls, "getPrimaryClip", "()Landroid/content/ClipData;");
    jmethodID count = env->GetMethodID(cd_cls, "getItemCount", "()I");
    jmethodID item_at = env->GetMethodID(cd_cls, "getItemAt", "(I)Landroid/content/ClipData$Item;");
    jmethodID coerce = env->GetMethodID(item_cls, "coerceToText", "(Landroid/content/Context;)Ljava/lang/CharSequence;");
    jmethodID to_string = env->GetMethodID(cs_cls, "toString", "()Ljava/lang/String;");
    if (Bad(env) || !current || !service || !primary || !count || !item_at || !coerce || !to_string) return false;
    jobject app = env->CallStaticObjectMethod(at, current);
    if (Bad(env) || !app) return false;
    jobject manager = env->CallObjectMethod(app, service, env->NewStringUTF("clipboard"));
    if (Bad(env) || !manager) return false;
    jobject clip = env->CallObjectMethod(manager, primary);
    if (Bad(env) || !clip || env->CallIntMethod(clip, count) < 1 || Bad(env)) return false;
    jobject item = env->CallObjectMethod(clip, item_at, 0);
    if (Bad(env) || !item) return false;
    jobject text = env->CallObjectMethod(item, coerce, app);
    if (Bad(env) || !text) return false;
    jstring str = static_cast<jstring>(env->CallObjectMethod(text, to_string));
    if (Bad(env) || !str) return false;
    const jsize n = env->GetStringLength(str);
    const jchar* chars = n > 0 ? env->GetStringChars(str, nullptr) : nullptr;
    if (n > 0 && !chars) return false;
    for (jsize i = 0; i < n && out.size() < 8192; ++i) { // UTF-16 -> standart UTF-8 (vekil çiftler dahil)
        uint32_t cp = chars[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < n && chars[i + 1] >= 0xDC00 && chars[i + 1] <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (chars[++i] - 0xDC00);
        else if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 63)); }
        else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
        else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 63)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
    }
    if (chars) env->ReleaseStringChars(str, chars);
    return true;
}

static bool PostWithTimeout(const std::string& url, const std::string& key, const std::string& body, int& code, std::string& response,int connect_ms,int read_ms) {
    code = 0; response.clear();
    if(body.size()>6u*1024u*1024u)return false;
    JavaVM* vm = FindVM();
    if (!vm) return false;
    Attach thread(vm);
    JNIEnv* env = thread.env;
    if (!env || env->PushLocalFrame(32) != 0) return false;
    struct Frame { JNIEnv* e; ~Frame() { e->PopLocalFrame(nullptr); } } frame{env};
    jclass url_cls = env->FindClass("java/net/URL");
    jclass http_cls = env->FindClass("java/net/HttpURLConnection");
    jclass out_cls = env->FindClass("java/io/OutputStream");
    jclass in_cls = env->FindClass("java/io/InputStream");
    if (Bad(env) || !url_cls || !http_cls || !out_cls || !in_cls) return false;
    jmethodID url_new = env->GetMethodID(url_cls, "<init>", "(Ljava/lang/String;)V");
    jmethodID open = env->GetMethodID(url_cls, "openConnection", "()Ljava/net/URLConnection;");
    jmethodID set_method = env->GetMethodID(http_cls, "setRequestMethod", "(Ljava/lang/String;)V");
    jmethodID set_prop = env->GetMethodID(http_cls, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
    jmethodID set_ct = env->GetMethodID(http_cls, "setConnectTimeout", "(I)V");
    jmethodID set_rt = env->GetMethodID(http_cls, "setReadTimeout", "(I)V");
    jmethodID set_out = env->GetMethodID(http_cls, "setDoOutput", "(Z)V");
    jmethodID get_out = env->GetMethodID(http_cls, "getOutputStream", "()Ljava/io/OutputStream;");
    jmethodID get_code = env->GetMethodID(http_cls, "getResponseCode", "()I");
    jmethodID get_in = env->GetMethodID(http_cls, "getInputStream", "()Ljava/io/InputStream;");
    jmethodID get_err = env->GetMethodID(http_cls, "getErrorStream", "()Ljava/io/InputStream;");
    jmethodID disconnect = env->GetMethodID(http_cls, "disconnect", "()V");
    jmethodID write = env->GetMethodID(out_cls, "write", "([B)V");
    jmethodID out_close = env->GetMethodID(out_cls, "close", "()V");
    jmethodID read = env->GetMethodID(in_cls, "read", "([B)I");
    jmethodID in_close = env->GetMethodID(in_cls, "close", "()V");
    if (Bad(env) || !url_new || !open || !set_method || !set_prop || !set_ct || !set_rt || !set_out || !get_out ||
        !get_code || !get_in || !get_err || !disconnect || !write || !out_close || !read || !in_close) return false;

    jstring jurl = env->NewStringUTF(url.c_str());
    jobject u = jurl ? env->NewObject(url_cls, url_new, jurl) : nullptr;
    if (Bad(env) || !u) return false;
    jobject conn = env->CallObjectMethod(u, open);
    if (Bad(env) || !conn) return false;
    auto str = [&](const char* s) { return env->NewStringUTF(s); };
    env->CallVoidMethod(conn, set_method, str("POST"));
    env->CallVoidMethod(conn, set_prop, str("Content-Type"), str("application/json; charset=utf-8"));
    env->CallVoidMethod(conn, set_prop, str("x-goog-api-key"), str(key.c_str()));
    if(url.find("/interactions")!=std::string::npos)env->CallVoidMethod(conn,set_prop,str("Api-Revision"),str("2026-05-20"));
    env->CallVoidMethod(conn, set_ct, connect_ms);
    env->CallVoidMethod(conn, set_rt, read_ms);
    env->CallVoidMethod(conn, set_out, JNI_TRUE);
    if (Bad(env)) return false;

    bool ok = false;
    jobject os = env->CallObjectMethod(conn, get_out);
    if (!Bad(env) && os) {
        jbyteArray arr = env->NewByteArray(static_cast<jsize>(body.size()));
        if (arr) {
            env->SetByteArrayRegion(arr, 0, static_cast<jsize>(body.size()), reinterpret_cast<const jbyte*>(body.data()));
            env->CallVoidMethod(os, write, arr);
            ok = !Bad(env);
        }
        env->CallVoidMethod(os, out_close); Bad(env);
    }
    if (!ok) { env->CallVoidMethod(conn, disconnect); Bad(env); return false; }

    code = env->CallIntMethod(conn, get_code);
    if (Bad(env)) { code = 0; env->CallVoidMethod(conn, disconnect); Bad(env); return false; }
    jobject is = env->CallObjectMethod(conn, code >= 400 ? get_err : get_in);
    if (Bad(env)) is = nullptr;
    if (is) {
        jbyteArray buffer = env->NewByteArray(4096);
        std::vector<jbyte> chunk(4096);
        while (buffer && response.size() < ((url.find("/interactions")!=std::string::npos||url.find("-tts")!=std::string::npos) ? (8u << 20) : (1u << 20))) {
            const jint n = env->CallIntMethod(is, read, buffer);
            if (Bad(env) || n <= 0) break;
            env->GetByteArrayRegion(buffer, 0, n, chunk.data());
            response.append(reinterpret_cast<const char*>(chunk.data()), static_cast<size_t>(n));
        }
        env->CallVoidMethod(is, in_close); Bad(env);
    }
    env->CallVoidMethod(conn, disconnect); Bad(env);
    return true;
}

bool Post(const std::string& url,const std::string& key,const std::string& body,int& code,std::string& response){return PostWithTimeout(url,key,body,code,response,8000,25000);}
bool PostVoice(const std::string& url,const std::string& key,const std::string& body,int& code,std::string& response){return PostWithTimeout(url,key,body,code,response,3500,8000);}

// GET without response-body parsing. The HTTPS certificate is verified by
// Android's platform trust store via HttpURLConnection.
bool GetStatus(const std::string& url, int& code) {
    code = 0;
    if (url != "https://api.github.com/repos/erensariisik03-sudo/-ekup-reposu") return false;
    JavaVM* vm = FindVM();
    if (!vm) return false;
    Attach thread(vm);
    JNIEnv* env = thread.env;
    if (!env || env->PushLocalFrame(24) != JNI_OK) return false;
    struct Frame { JNIEnv* e; ~Frame() { e->PopLocalFrame(nullptr); } } frame{env};
    jclass url_cls = env->FindClass("java/net/URL");
    jclass http_cls = env->FindClass("java/net/HttpURLConnection");
    if (Bad(env) || !url_cls || !http_cls) return false;
    jmethodID make_url = env->GetMethodID(url_cls, "<init>", "(Ljava/lang/String;)V");
    jmethodID open = env->GetMethodID(url_cls, "openConnection", "()Ljava/net/URLConnection;");
    jmethodID set_method = env->GetMethodID(http_cls, "setRequestMethod", "(Ljava/lang/String;)V");
    jmethodID set_header = env->GetMethodID(http_cls, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
    jmethodID set_connect = env->GetMethodID(http_cls, "setConnectTimeout", "(I)V");
    jmethodID set_read = env->GetMethodID(http_cls, "setReadTimeout", "(I)V");
    jmethodID get_code = env->GetMethodID(http_cls, "getResponseCode", "()I");
    jmethodID disconnect = env->GetMethodID(http_cls, "disconnect", "()V");
    if (Bad(env) || !make_url || !open || !set_method || !set_header || !set_connect || !set_read || !get_code || !disconnect) return false;
    jstring target = env->NewStringUTF(url.c_str());
    jobject u = target ? env->NewObject(url_cls, make_url, target) : nullptr;
    if (Bad(env) || !u) return false;
    jobject conn = env->CallObjectMethod(u, open);
    if (Bad(env) || !conn) return false;
    auto str = [&](const char* text) { return env->NewStringUTF(text); };
    env->CallVoidMethod(conn, set_method, str("GET"));
    env->CallVoidMethod(conn, set_header, str("Accept"), str("application/vnd.github+json"));
    env->CallVoidMethod(conn, set_header, str("User-Agent"), str("GMM-Repo-Status-Check"));
    env->CallVoidMethod(conn, set_connect, 5000);
    env->CallVoidMethod(conn, set_read, 5000);
    if (Bad(env)) { env->CallVoidMethod(conn, disconnect); Bad(env); return false; }
    code = env->CallIntMethod(conn, get_code);
    const bool ok = !Bad(env);
    if (!ok) code = 0;
    env->CallVoidMethod(conn, disconnect); Bad(env);
    return ok;
}

// ---- Yerel Android TTS (Termux:API'nin kullandığı TextToSpeech motoru) ---------------------------
// Ağ ve kota yok. Dosyaya sentezlenir (synthesizeToFile), WAV olarak okunur. Yalnız arka plan
// thread'inde çalışır; Unity/IL2CPP nesnelerine dokunmaz. Metin loglanmaz.
namespace {
std::mutex g_tts_mutex;
jobject g_tts = nullptr;                 // global ref: TextToSpeech
bool g_tts_ready = false;
std::string g_selected_voice="\x01";
long long g_tts_retry_ms = 0;            // başarısız başlatmadan sonra bu ana kadar tekrar deneme
long long MsNow() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
#define TTS_LOG(...) __android_log_print(ANDROID_LOG_INFO, "multiplayermod", __VA_ARGS__)
}
bool Speak(const std::string& text, std::string& wav) {
    wav.clear();
    if (text.size() > 2000) return false;
    std::lock_guard<std::mutex> lock(g_tts_mutex);
    if (!g_tts_ready && MsNow() < g_tts_retry_ms) return false;
    JavaVM* vm = FindVM();
    if (!vm) return false;
    Attach thread(vm);
    JNIEnv* env = thread.env;
    if (!env || env->PushLocalFrame(64) != 0) return false;
    struct Frame { JNIEnv* e; ~Frame() { e->PopLocalFrame(nullptr); } } frame{env};
    jclass at = env->FindClass("android/app/ActivityThread");
    jclass tts_cls = env->FindClass("android/speech/tts/TextToSpeech");
    jclass loc_cls = env->FindClass("java/util/Locale");
    jclass ctx_cls = env->FindClass("android/content/Context");
    jclass file_cls = env->FindClass("java/io/File");
    jclass cs_cls = env->FindClass("java/lang/CharSequence");
    if (Bad(env) || !at || !tts_cls || !loc_cls || !ctx_cls || !file_cls || !cs_cls) return false;
    jmethodID current = env->GetStaticMethodID(at, "currentApplication", "()Landroid/app/Application;");
    jmethodID tts_new = env->GetMethodID(tts_cls, "<init>", "(Landroid/content/Context;Landroid/speech/tts/TextToSpeech$OnInitListener;)V");
    jmethodID loc_new = env->GetMethodID(loc_cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
    jmethodID avail = env->GetMethodID(tts_cls, "isLanguageAvailable", "(Ljava/util/Locale;)I");
    jmethodID set_lang = env->GetMethodID(tts_cls, "setLanguage", "(Ljava/util/Locale;)I");
    jmethodID set_pitch = env->GetMethodID(tts_cls, "setPitch", "(F)I");
    jmethodID set_rate = env->GetMethodID(tts_cls, "setSpeechRate", "(F)I");
    jmethodID synth = env->GetMethodID(tts_cls, "synthesizeToFile", "(Ljava/lang/CharSequence;Landroid/os/Bundle;Ljava/io/File;Ljava/lang/String;)I");
    jmethodID cache_dir = env->GetMethodID(ctx_cls, "getCacheDir", "()Ljava/io/File;");
    jmethodID file_new = env->GetMethodID(file_cls, "<init>", "(Ljava/io/File;Ljava/lang/String;)V");
    jmethodID file_path = env->GetMethodID(file_cls, "getAbsolutePath", "()Ljava/lang/String;");
    jmethodID file_del = env->GetMethodID(file_cls, "delete", "()Z");
    if (Bad(env) || !current || !tts_new || !loc_new || !avail || !set_lang || !set_pitch || !set_rate || !synth || !cache_dir || !file_new || !file_path || !file_del) return false;
    jobject app = env->CallStaticObjectMethod(at, current);
    if (Bad(env) || !app) return false;
    jstring tr = env->NewStringUTF("tr"), tr_cc = env->NewStringUTF("TR");
    jobject locale = env->NewObject(loc_cls, loc_new, tr, tr_cc);
    if (Bad(env) || !locale) return false;

    if (!g_tts_ready) {
        if (g_tts) { env->DeleteGlobalRef(g_tts); g_tts = nullptr; }
        jobject local = env->NewObject(tts_cls, tts_new, app, static_cast<jobject>(nullptr)); // dinleyici yok; hazır olmayı yoklarız
        if (Bad(env) || !local) { TTS_LOG("GMM_DIAG tts init-failed"); g_tts_retry_ms = MsNow() + 30000; return false; }
        g_tts = env->NewGlobalRef(local);
        int code = -2;
        for (int i = 0; i < 80; ++i) { // en çok ~8 sn: servis bağlanana kadar isLanguageAvailable hata (-2) döner
            code = env->CallIntMethod(g_tts, avail, locale);
            if (Bad(env)) code = -2;
            if (code >= 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        TTS_LOG("GMM_DIAG tts language-tr=%d", code);
        if (code < 0) { env->DeleteGlobalRef(g_tts); g_tts = nullptr; g_tts_retry_ms = MsNow() + 30000; return false; } // motor yok / Türkçe yüklü değil / paket görünürlüğü
        env->CallIntMethod(g_tts, set_lang, locale); Bad(env);
        g_selected_voice="\x01";
        g_tts_ready = true;
    }

    const auto preferred=voice_ai_bridge::PreferredVoice();
    if(g_selected_voice!=preferred){
        g_selected_voice=preferred;
        std::vector<std::string> names;std::string actual;
        jclass vc=env->FindClass("android/speech/tts/Voice"),sc=env->FindClass("java/util/Set"),ic=env->FindClass("java/util/Iterator");
        if(!Bad(env)&&vc&&sc&&ic){
            auto voices=env->GetMethodID(tts_cls,"getVoices","()Ljava/util/Set;");auto get_voice=env->GetMethodID(tts_cls,"getVoice","()Landroid/speech/tts/Voice;");auto set_voice=env->GetMethodID(tts_cls,"setVoice","(Landroid/speech/tts/Voice;)I");
            auto iter=env->GetMethodID(sc,"iterator","()Ljava/util/Iterator;");auto more=env->GetMethodID(ic,"hasNext","()Z");auto next=env->GetMethodID(ic,"next","()Ljava/lang/Object;");
            auto name_id=env->GetMethodID(vc,"getName","()Ljava/lang/String;");auto loc_id=env->GetMethodID(vc,"getLocale","()Ljava/util/Locale;");auto lang_id=env->GetMethodID(loc_cls,"getLanguage","()Ljava/lang/String;");auto network_id=env->GetMethodID(vc,"isNetworkConnectionRequired","()Z");
            if(!Bad(env)&&voices&&get_voice&&set_voice&&iter&&more&&next&&name_id&&loc_id&&lang_id&&network_id){
                auto strval=[&](jstring v){std::string out;if(v){const char* p=env->GetStringUTFChars(v,nullptr);if(p){out=p;env->ReleaseStringUTFChars(v,p);}env->DeleteLocalRef(v);}Bad(env);return out;};
                env->CallIntMethod(g_tts,set_lang,locale);Bad(env);
                jobject set=env->CallObjectMethod(g_tts,voices);jobject it=(!Bad(env)&&set)?env->CallObjectMethod(set,iter):nullptr;
                int best=-1; jobject picked=nullptr;
                for(int n=0;it&&n<500;++n){if(!env->CallBooleanMethod(it,more)||Bad(env))break;jobject v=env->CallObjectMethod(it,next);if(Bad(env)||!v)break;
                    jobject loc=env->CallObjectMethod(v,loc_id);std::string lang=loc?strval(static_cast<jstring>(env->CallObjectMethod(loc,lang_id))):"";if(loc)env->DeleteLocalRef(loc);
                    if(lang=="tr"){
                        const auto name=strval(static_cast<jstring>(env->CallObjectMethod(v,name_id)));bool network=env->CallBooleanMethod(v,network_id);Bad(env);
                        if(!name.empty()&&!network){names.push_back(name);auto lower=name;for(char& c:lower)c=char(std::tolower(static_cast<unsigned char>(c)));bool male=(lower.find("male")!=std::string::npos&&lower.find("female")==std::string::npos)||lower.find("erkek")!=std::string::npos;int score=(!preferred.empty()&&name==preferred)?3:(preferred.empty()&&male?2:0);if(score>best){if(picked)env->DeleteLocalRef(picked);picked=env->NewLocalRef(v);best=score;}}
                    }env->DeleteLocalRef(v);
                }
                if(picked){env->CallIntMethod(g_tts,set_voice,picked);Bad(env);env->DeleteLocalRef(picked);}
                jobject voice=env->CallObjectMethod(g_tts,get_voice);if(!Bad(env)&&voice)actual=strval(static_cast<jstring>(env->CallObjectMethod(voice,name_id)));
            }
        }
        voice_ai_bridge::PublishVoices(std::move(names),actual);
    }
    env->CallIntMethod(g_tts,set_pitch,.75f+.005f*voice_ai_bridge::tone.load());Bad(env);
    env->CallIntMethod(g_tts,set_rate,1.05f);Bad(env);
    if(text.empty())return true; // prewarm / refresh voice catalogue only
    jobject dir = env->CallObjectMethod(app, cache_dir);
    const std::string unique="gmm_tts_"+std::to_string(MsNow())+".wav";
    jstring name = env->NewStringUTF(unique.c_str());
    jobject file = (!Bad(env) && dir) ? env->NewObject(file_cls, file_new, dir, name) : nullptr;
    if (Bad(env) || !file) return false;
    jstring jpath = static_cast<jstring>(env->CallObjectMethod(file, file_path));
    if (Bad(env) || !jpath) return false;
    const char* c_path = env->GetStringUTFChars(jpath, nullptr);
    const std::string path = c_path ? c_path : "";
    if (c_path) env->ReleaseStringUTFChars(jpath, c_path);
    if (path.empty()) return false;
    env->CallBooleanMethod(file, file_del); Bad(env);
    const auto wide=Utf8ToUtf16(text);
    jstring jtext = env->NewString(reinterpret_cast<const jchar*>(wide.data()),static_cast<jsize>(wide.size()));
    jstring utt = env->NewStringUTF(unique.c_str());
    if (Bad(env) || !jtext) return false;
    const jint queued = env->CallIntMethod(g_tts, synth, jtext, static_cast<jobject>(nullptr), file, utt);
    if (Bad(env) || queued != 0) { TTS_LOG("GMM_DIAG tts synth-rejected=%d", queued); g_tts_ready = false; return false; }
    // Tam WAV başlığı + sabit boyut; tamamlanmamış dosyayı okumayız. En çok 12 sn.
    long long last_size = -1, stable_since = 0; const long long start = MsNow();
    while (MsNow() - start < 12000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in) continue;
        const long long size = static_cast<long long>(in.tellg());
        if (size > 44 && size == last_size) { if (!stable_since) stable_since = MsNow(); if (MsNow() - stable_since >= 150){unsigned char h[12]{};in.seekg(0);in.read(reinterpret_cast<char*>(h),12);uint64_t total=8ull+h[4]+(uint64_t(h[5])<<8)+(uint64_t(h[6])<<16)+(uint64_t(h[7])<<24);if(in&&h[0]=='R'&&h[1]=='I'&&h[2]=='F'&&h[3]=='F'&&total==uint64_t(size))break;} }
        else { stable_since = 0; last_size = size; }
    }
    const bool completed=MsNow()-start<12000;
    if(!completed){auto stop=env->GetMethodID(tts_cls,"stop","()I");if(!Bad(env)&&stop){env->CallIntMethod(g_tts,stop);Bad(env);}}
    std::ifstream in(path, std::ios::binary);
    if (completed&&in) {
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (data.size() > 44 && data.size() < 6u * 1024u * 1024u) wav = std::move(data);
    }
    env->CallBooleanMethod(file, file_del); Bad(env);
    TTS_LOG("GMM_DIAG tts wav_bytes=%zu", wav.size());
    return !wav.empty();
}
#if defined(__ANDROID__)
namespace {
bool GetMedia(const std::string& url,size_t limit,std::string& bytes){
 bytes.clear();if(url.rfind("https://www.myinstants.com/",0)!=0)return false;
 auto* vm=FindVM();if(!vm)return false;Attach thread(vm);auto* env=thread.env;if(!env||env->PushLocalFrame(32)!=0)return false;
 struct Frame{JNIEnv* e;~Frame(){e->PopLocalFrame(nullptr);}} frame{env};
 auto uc=env->FindClass("java/net/URL"),hc=env->FindClass("java/net/HttpURLConnection"),ic=env->FindClass("java/io/InputStream");if(Bad(env)||!uc||!hc||!ic)return false;
 auto uctor=env->GetMethodID(uc,"<init>","(Ljava/lang/String;)V"),open=env->GetMethodID(uc,"openConnection","()Ljava/net/URLConnection;");
 auto ct=env->GetMethodID(hc,"setConnectTimeout","(I)V"),rt=env->GetMethodID(hc,"setReadTimeout","(I)V"),redirect=env->GetMethodID(hc,"setInstanceFollowRedirects","(Z)V"),header=env->GetMethodID(hc,"setRequestProperty","(Ljava/lang/String;Ljava/lang/String;)V"),code=env->GetMethodID(hc,"getResponseCode","()I"),input=env->GetMethodID(hc,"getInputStream","()Ljava/io/InputStream;"),disc=env->GetMethodID(hc,"disconnect","()V");
 auto read=env->GetMethodID(ic,"read","([B)I"),close=env->GetMethodID(ic,"close","()V");if(Bad(env)||!uctor||!open||!ct||!rt||!redirect||!header||!code||!input||!disc||!read||!close)return false;
 auto u=env->NewObject(uc,uctor,env->NewStringUTF(url.c_str()));if(Bad(env)||!u)return false;auto con=env->CallObjectMethod(u,open);if(Bad(env)||!con)return false;
 env->CallVoidMethod(con,ct,4000);env->CallVoidMethod(con,rt,5000);env->CallVoidMethod(con,redirect,JNI_FALSE);env->CallVoidMethod(con,header,env->NewStringUTF("User-Agent"),env->NewStringUTF("Mozilla/5.0 (Android)"));
 bool ok=false;auto until=MsNow()+12000;
 if(!Bad(env)&&env->CallIntMethod(con,code)==200&&!Bad(env)){
  auto in=env->CallObjectMethod(con,input);if(!Bad(env)&&in){auto buf=env->NewByteArray(8192);std::vector<jbyte> chunk(8192);
   while(buf&&MsNow()<until){jint n=env->CallIntMethod(in,read,buf);if(Bad(env))break;if(n<0){ok=true;break;}if(n==0)continue;if(n>8192||bytes.size()+size_t(n)>limit)break;env->GetByteArrayRegion(buf,0,n,chunk.data());if(Bad(env))break;bytes.append(reinterpret_cast<char*>(chunk.data()),size_t(n));}
   env->CallVoidMethod(in,close);Bad(env);
  }
 }Bad(env);env->CallVoidMethod(con,disc);Bad(env);if(!ok)bytes.clear();return ok&&!bytes.empty();
}
struct Fd{int fd=-1;~Fd(){if(fd>=0)::close(fd);}};
int TemporaryAudioFd(){
#if defined(SYS_memfd_create)
 int fd=int(syscall(SYS_memfd_create,"gmm_sound",1u));if(fd>=0)return fd;
#endif
 auto* vm=FindVM();if(!vm)return -1;Attach thread(vm);auto* env=thread.env;if(!env||env->PushLocalFrame(16)!=0)return -1;
 struct Frame{JNIEnv* e;~Frame(){e->PopLocalFrame(nullptr);}} frame{env};
 auto at=env->FindClass("android/app/ActivityThread"),ctx=env->FindClass("android/content/Context"),file=env->FindClass("java/io/File");if(Bad(env)||!at||!ctx||!file)return -1;
 auto current=env->GetStaticMethodID(at,"currentApplication","()Landroid/app/Application;"),cache=env->GetMethodID(ctx,"getCacheDir","()Ljava/io/File;"),path=env->GetMethodID(file,"getAbsolutePath","()Ljava/lang/String;");if(Bad(env)||!current||!cache||!path)return -1;
 auto app=env->CallStaticObjectMethod(at,current);if(Bad(env)||!app)return -1;auto dir=env->CallObjectMethod(app,cache);if(Bad(env)||!dir)return -1;auto jpath=static_cast<jstring>(env->CallObjectMethod(dir,path));if(Bad(env)||!jpath)return -1;
 const char* chars=env->GetStringUTFChars(jpath,nullptr);if(!chars)return -1;std::string pattern=std::string(chars)+"/gmm_sound_XXXXXX";env->ReleaseStringUTFChars(jpath,chars);std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);int result=mkstemp(name.data());if(result>=0)unlink(name.data());return result;
}
struct Extractor{AMediaExtractor* p=AMediaExtractor_new();~Extractor(){if(p)AMediaExtractor_delete(p);}};
struct Format{AMediaFormat* p=nullptr;~Format(){if(p)AMediaFormat_delete(p);}};
struct Codec{AMediaCodec* p=nullptr;bool started=false;~Codec(){if(p){if(started)AMediaCodec_stop(p);AMediaCodec_delete(p);}}};
bool DecodeMp3(const std::string& bytes,std::string& wav){
 // The file lives in RAM; Android's decoder reads only this file descriptor.
 Fd f;f.fd=TemporaryAudioFd();if(f.fd<0)return false;
 size_t offset=0;while(offset<bytes.size()){ssize_t n=::write(f.fd,bytes.data()+offset,bytes.size()-offset);if(n<=0)return false;offset+=size_t(n);}
 Extractor ex;if(!ex.p||AMediaExtractor_setDataSourceFd(ex.p,f.fd,0,bytes.size())!=AMEDIA_OK)return false;
 Codec decoder;Format selected;int32_t rate=0,channels=0;
 for(size_t i=0;i<AMediaExtractor_getTrackCount(ex.p);++i){Format fmt{AMediaExtractor_getTrackFormat(ex.p,i)};const char* mime=nullptr;if(!fmt.p||!AMediaFormat_getString(fmt.p,"mime",&mime)||!mime||std::string(mime)!="audio/mpeg")continue;
  if(AMediaExtractor_selectTrack(ex.p,i)!=AMEDIA_OK)return false;decoder.p=AMediaCodec_createDecoderByType(mime);selected.p=fmt.p;fmt.p=nullptr;break;}
 if(!decoder.p||!selected.p||AMediaCodec_configure(decoder.p,selected.p,nullptr,nullptr,0)!=AMEDIA_OK||AMediaCodec_start(decoder.p)!=AMEDIA_OK)return false;decoder.started=true;
 AMediaFormat_getInt32(selected.p,"sample-rate",&rate);AMediaFormat_getInt32(selected.p,"channel-count",&channels);
 bool input_end=false,output_end=false;std::vector<float> pcm;const auto deadline=MsNow()+10000;
 while(!output_end&&MsNow()<deadline){
  if(!input_end){ssize_t index=AMediaCodec_dequeueInputBuffer(decoder.p,5000);if(index>=0){size_t cap=0;auto* buf=AMediaCodec_getInputBuffer(decoder.p,size_t(index),&cap);if(!buf)return false;ssize_t size=AMediaExtractor_readSampleData(ex.p,buf,cap);if(size<0){if(AMediaCodec_queueInputBuffer(decoder.p,size_t(index),0,0,0,AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)!=AMEDIA_OK)return false;input_end=true;}else{if(size_t(size)>cap||AMediaCodec_queueInputBuffer(decoder.p,size_t(index),0,size_t(size),std::max<int64_t>(0,AMediaExtractor_getSampleTime(ex.p)),0)!=AMEDIA_OK)return false;AMediaExtractor_advance(ex.p);}}}
  AMediaCodecBufferInfo info{};ssize_t index=AMediaCodec_dequeueOutputBuffer(decoder.p,&info,5000);
  if(index==AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED){Format fmt{AMediaCodec_getOutputFormat(decoder.p)};if(!fmt.p)return false;AMediaFormat_getInt32(fmt.p,"sample-rate",&rate);AMediaFormat_getInt32(fmt.p,"channel-count",&channels);int32_t encoding=2;if(AMediaFormat_getInt32(fmt.p,"pcm-encoding",&encoding)&&encoding!=2)return false;}
  else if(index>=0){size_t cap=0;auto* buf=AMediaCodec_getOutputBuffer(decoder.p,size_t(index),&cap);if(rate<8000||rate>48000||channels<1||channels>2||info.offset<0||info.size<0||size_t(info.offset)>cap||size_t(info.size)>cap-size_t(info.offset)||(info.size&&!buf)||info.size%(channels*2))return false;
   const auto* b=buf?buf+info.offset:nullptr;for(size_t i=0;i<size_t(info.size)&&pcm.size()<size_t(rate)*15;i+=channels*2){float x=0;for(int c=0;c<channels;++c){size_t j=i+c*2;uint16_t u=uint16_t(b[j])|(uint16_t(b[j+1])<<8);x+=int16_t(u)/32768.f;}pcm.push_back(x/channels*.65f);}
   output_end=(info.flags&AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)||pcm.size()>=size_t(rate)*15;AMediaCodec_releaseOutputBuffer(decoder.p,size_t(index),false);
  }
 }
 if(!output_end||pcm.empty())return false;wav=voice_ai_core::Wav(pcm,rate);return true;
}
}
std::vector<myinstants::Sound> SearchSounds(const std::string& query){std::string html;if(query.empty()||query.size()>240||!GetMedia("https://www.myinstants.com/en/search/?name="+myinstants::Encode(query),2u<<20,html))return {};return myinstants::Parse(html);}
bool DownloadSound(const std::string& path,std::string& wav){std::string bytes;return myinstants::ValidPath(path)&&GetMedia("https://www.myinstants.com"+path,4u<<20,bytes)&&DecodeMp3(bytes,wav);}
#endif

}
