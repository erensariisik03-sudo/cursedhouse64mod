#include "chat_bot_http.h"
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

bool Post(const std::string& url, const std::string& key, const std::string& body, int& code, std::string& response) {
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
    env->CallVoidMethod(conn, set_ct, 8000);
    env->CallVoidMethod(conn, set_rt, 25000);
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
bool g_tts_ready = false, g_voice_done = false;
long long g_tts_retry_ms = 0;            // başarısız başlatmadan sonra bu ana kadar tekrar deneme
long long MsNow() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
#define TTS_LOG(...) __android_log_print(ANDROID_LOG_INFO, "multiplayermod", __VA_ARGS__)
}
bool Speak(const std::string& text, std::string& wav) {
    wav.clear();
    if (text.empty() || text.size() > 2000) return false;
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
        // Erkek ton: Android'de cinsiyet alanı yok; perdeyi düşür, hızı biraz yavaşlat.
        env->CallIntMethod(g_tts, set_pitch, 0.6f); Bad(env);
        env->CallIntMethod(g_tts, set_rate, 0.95f); Bad(env);
        g_tts_ready = true;
    }

    jobject dir = env->CallObjectMethod(app, cache_dir);
    jstring name = env->NewStringUTF("gmm_tts.wav");
    jobject file = (!Bad(env) && dir) ? env->NewObject(file_cls, file_new, dir, name) : nullptr;
    if (Bad(env) || !file) return false;
    jstring jpath = static_cast<jstring>(env->CallObjectMethod(file, file_path));
    if (Bad(env) || !jpath) return false;
    const char* c_path = env->GetStringUTFChars(jpath, nullptr);
    const std::string path = c_path ? c_path : "";
    if (c_path) env->ReleaseStringUTFChars(jpath, c_path);
    if (path.empty()) return false;
    env->CallBooleanMethod(file, file_del); Bad(env);
    jstring jtext = env->NewStringUTF(text.c_str());
    jstring utt = env->NewStringUTF("gmm");
    if (Bad(env) || !jtext) return false;
    const jint queued = env->CallIntMethod(g_tts, synth, jtext, static_cast<jobject>(nullptr), file, utt);
    if (Bad(env) || queued != 0) { TTS_LOG("GMM_DIAG tts synth-rejected=%d", queued); g_tts_ready = false; return false; }
    // Tamamlanma: dosya var, >44 bayt ve 350 ms boyunca büyümüyor. En çok 20 sn.
    long long last_size = -1, stable_since = 0; const long long start = MsNow();
    while (MsNow() - start < 20000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in) continue;
        const long long size = static_cast<long long>(in.tellg());
        if (size > 44 && size == last_size) { if (!stable_since) stable_since = MsNow(); if (MsNow() - stable_since >= 350) break; }
        else { stable_since = 0; last_size = size; }
    }
    std::ifstream in(path, std::ios::binary);
    if (in) {
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (data.size() > 44 && data.size() < 6u * 1024u * 1024u) wav = std::move(data);
    }
    env->CallBooleanMethod(file, file_del); Bad(env);
    TTS_LOG("GMM_DIAG tts wav_bytes=%zu", wav.size());
    return !wav.empty();
}
}
