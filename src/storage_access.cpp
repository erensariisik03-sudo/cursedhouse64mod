#include "storage_access.h"
#ifdef __ANDROID__
#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <string>
#include <initializer_list>
#include <atomic>

namespace storage_access {
namespace {
std::atomic<JavaVM*> g_vm{nullptr};
JavaVM* FindVM() {
    if (JavaVM* vm = g_vm.load()) return vm;
    using Getter = jint (*)(JavaVM**, jsize, jsize*);
    for (const char* path : {static_cast<const char*>(nullptr), "libnativehelper.so", "libart.so"}) {
        void* handle = path ? dlopen(path, RTLD_NOW | RTLD_NOLOAD) : RTLD_DEFAULT;
        if (!handle) continue;
        auto fn = reinterpret_cast<Getter>(dlsym(handle, "JNI_GetCreatedJavaVMs"));
        JavaVM* vm = nullptr;
        jsize count = 0;
        if (fn && fn(&vm, 1, &count) == JNI_OK && count > 0 && vm) { g_vm.store(vm); return vm; }
    }
    return nullptr;
}
template<class VM> auto Attach(VM* vm, JNIEnv** env, int)
    -> decltype(vm->AttachCurrentThread(env, nullptr)) {
    return vm->AttachCurrentThread(env, nullptr);
}
template<class VM> jint Attach(VM* vm, JNIEnv** env, long) {
    return vm->AttachCurrentThread(reinterpret_cast<void**>(env), nullptr);
}
struct Env {
    JavaVM* vm = FindVM();
    JNIEnv* env = nullptr;
    bool attached = false;
    Env() {
        if (!vm) return;
        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return;
        if (Attach(vm, &env, 0) == JNI_OK) attached = true;
        else env = nullptr;
    }
    ~Env() { if (attached && vm) vm->DetachCurrentThread(); }
};
bool Failed(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}
int Sdk(JNIEnv* env) {
    jclass version = env->FindClass("android/os/Build$VERSION");
    if (!version || Failed(env)) return 0;
    jfieldID field = env->GetStaticFieldID(version, "SDK_INT", "I");
    if (!field || Failed(env)) return 0;
    return env->GetStaticIntField(version, field);
}
} // namespace

void SetVM(JavaVM* vm) { g_vm.store(vm); }

bool Granted() {
    Env thread;
    JNIEnv* env = thread.env;
    if (!env || env->PushLocalFrame(16) != JNI_OK) return false;
    bool granted = false;
    if (Sdk(env) >= 30) {
        jclass cls = env->FindClass("android/os/Environment");
        if (cls && !Failed(env)) {
            jmethodID check = env->GetStaticMethodID(cls, "isExternalStorageManager", "()Z");
            if (check && !Failed(env)) granted = env->CallStaticBooleanMethod(cls, check) == JNI_TRUE;
        }
    }
    Failed(env);
    env->PopLocalFrame(nullptr);
    return granted;
}

bool OpenSettings() {
    Env thread;
    JNIEnv* env = thread.env;
    if (!env || env->PushLocalFrame(40) != JNI_OK) return false;
    bool ok = false;
    do {
        if (Sdk(env) < 30) break;
        // Use the running application's own context. The settings page is
        // system controlled and explicitly requires a human to grant access.
        jclass at = env->FindClass("android/app/ActivityThread");
        if (!at || Failed(env)) break;
        jmethodID current = env->GetStaticMethodID(at, "currentApplication", "()Landroid/app/Application;");
        if (!current || Failed(env)) break;
        jobject app = env->CallStaticObjectMethod(at, current);
        if (!app || Failed(env)) break;
        jclass context = env->FindClass("android/content/Context");
        jclass uri = env->FindClass("android/net/Uri");
        jclass intent = env->FindClass("android/content/Intent");
        if (!context || !uri || !intent || Failed(env)) break;
        jmethodID pkgMethod = env->GetMethodID(context, "getPackageName", "()Ljava/lang/String;");
        jmethodID parse = env->GetStaticMethodID(uri, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
        jmethodID ctor = env->GetMethodID(intent, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
        jmethodID addFlags = env->GetMethodID(intent, "addFlags", "(I)Landroid/content/Intent;");
        jmethodID start = env->GetMethodID(context, "startActivity", "(Landroid/content/Intent;)V");
        if (!pkgMethod || !parse || !ctor || !addFlags || !start || Failed(env)) break;
        jstring pkg = static_cast<jstring>(env->CallObjectMethod(app, pkgMethod));
        if (!pkg || Failed(env)) break;
        const char* text = env->GetStringUTFChars(pkg, nullptr);
        if (!text || Failed(env)) break;
        std::string packageUri = "package:";
        packageUri += text;
        env->ReleaseStringUTFChars(pkg, text);
        jstring uriText = env->NewStringUTF(packageUri.c_str());
        jobject parsed = env->CallStaticObjectMethod(uri, parse, uriText);
        jstring action = env->NewStringUTF("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION");
        if (!parsed || !action || Failed(env)) break;
        jobject request = env->NewObject(intent, ctor, action, parsed);
        if (!request || Failed(env)) break;
        // Application context requires the NEW_TASK flag.
        env->CallObjectMethod(request, addFlags, static_cast<jint>(0x10000000));
        if (Failed(env)) break;
        env->CallVoidMethod(app, start, request);
        ok = !Failed(env);
    } while (false);
    if (!ok) {
        Failed(env);
        __android_log_print(ANDROID_LOG_WARN, "multiplayermod", "GMM storage settings unavailable");
    }
    env->PopLocalFrame(nullptr);
    return ok;
}
} // namespace storage_access
#endif
