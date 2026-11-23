#include "seda_assets.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace seda_assets {
namespace {
std::atomic<JavaVM*> g_vm{nullptr};
std::mutex g_mutex;

JavaVM* FindVM() {
    if (auto* vm = g_vm.load()) return vm;
    using Getter = jint (*)(JavaVM**, jsize, jsize*);
    for (const char* lib : {static_cast<const char*>(nullptr), "libnativehelper.so", "libart.so"}) {
        void* handle = lib ? dlopen(lib, RTLD_NOW | RTLD_NOLOAD) : RTLD_DEFAULT;
        if (!handle) continue;
        auto get = reinterpret_cast<Getter>(dlsym(handle, "JNI_GetCreatedJavaVMs"));
        JavaVM* vm = nullptr;
        jsize count = 0;
        if (get && get(&vm, 1, &count) == JNI_OK && count > 0 && vm) {
            g_vm.store(vm);
            return vm;
        }
    }
    return nullptr;
}

// The Android NDK JNI signature uses JNIEnv**, desktop JDK uses void**.
template<class V> auto AttachImpl(V* vm, JNIEnv** env, int)
    -> decltype(vm->AttachCurrentThread(env, nullptr)) {
    return vm->AttachCurrentThread(env, nullptr);
}
template<class V> jint AttachImpl(V* vm, JNIEnv** env, long) {
    return vm->AttachCurrentThread(reinterpret_cast<void**>(env), nullptr);
}
struct ThreadEnv {
    JavaVM* vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    explicit ThreadEnv(JavaVM* v) : vm(v) {
        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return;
        if (AttachImpl(vm, &env, 0) == JNI_OK) attached = true;
        else env = nullptr;
    }
    ~ThreadEnv() { if (attached) vm->DetachCurrentThread(); }
};

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

bool MakeDir(const std::string& path) {
    if (::mkdir(path.c_str(), 0700) == 0) return true;
    struct stat st{};
    return errno == EEXIST && ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string ReadDiskText(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return {};
    std::string s(256, '\0');
    in.read(&s[0], static_cast<std::streamsize>(s.size()));
    s.resize(static_cast<size_t>(in.gcount()));
    return s;
}

std::string ReadAssetText(AAssetManager* manager, const std::string& asset_name) {
    AAsset* asset = AAssetManager_open(manager, asset_name.c_str(), AASSET_MODE_STREAMING);
    if (!asset) return {};
    char buffer[256]{};
    const int count = AAsset_read(asset, buffer, sizeof(buffer));
    AAsset_close(asset);
    return count > 0 && count < static_cast<int>(sizeof(buffer)) ? std::string(buffer, count) : std::string{};
}

bool ValidSize(AAssetManager* manager, const std::string& base, const std::string& rel) {
    AAsset* asset = AAssetManager_open(manager, ("gmm_seda/" + rel).c_str(), AASSET_MODE_STREAMING);
    if (!asset) return false;
    const off64_t size = AAsset_getLength64(asset);
    AAsset_close(asset);
    struct stat st{};
    return size > 0 && ::stat((base + "/" + rel).c_str(), &st) == 0 &&
           S_ISREG(st.st_mode) && st.st_size == size;
}

bool CopyAsset(AAssetManager* manager, const std::string& rel, const std::string& dest) {
    AAsset* asset = AAssetManager_open(manager, ("gmm_seda/" + rel).c_str(), AASSET_MODE_STREAMING);
    if (!asset) return false;
    const off64_t expected = AAsset_getLength64(asset);
    // These are tiny-streaming model parts: refuse unexpectedly huge/missing assets.
    if (expected <= 0 || expected > 32LL * 1024 * 1024) { AAsset_close(asset); return false; }
    std::string pattern = dest + ".tmp-XXXXXX";
    std::vector<char> tmp(pattern.begin(), pattern.end());
    tmp.push_back('\0');
    int fd = ::mkstemp(tmp.data());
    if (fd < 0) { AAsset_close(asset); return false; }
    bool ok = true;
    off64_t total = 0;
    unsigned char buf[65536];
    while (ok) {
        const int count = AAsset_read(asset, buf, sizeof(buf));
        if (count < 0) { ok = false; break; }
        if (count == 0) break;
        total += count;
        if (total > expected) { ok = false; break; }
        size_t sent = 0;
        while (sent < static_cast<size_t>(count)) {
            const ssize_t n = ::write(fd, buf + sent, static_cast<size_t>(count) - sent);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) { ok = false; break; }
            sent += static_cast<size_t>(n);
        }
    }
    AAsset_close(asset);
    ok = ok && total == expected && ::fsync(fd) == 0;
    if (::close(fd) != 0) ok = false;
    if (ok && ::rename(tmp.data(), dest.c_str()) != 0) ok = false;
    if (!ok) ::unlink(tmp.data());
    return ok;
}

bool WriteVersion(const std::string& name, const std::string& version) {
    std::string pattern = name + ".tmp-XXXXXX";
    std::vector<char> tmp(pattern.begin(), pattern.end());
    tmp.push_back('\0');
    int fd = ::mkstemp(tmp.data());
    if (fd < 0) return false;
    size_t pos = 0;
    bool ok = true;
    while (pos < version.size()) {
        ssize_t written = ::write(fd, version.data() + pos, version.size() - pos);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { ok = false; break; }
        pos += static_cast<size_t>(written);
    }
    ok = ok && ::fsync(fd) == 0;
    if (::close(fd) != 0) ok = false;
    if (ok && ::rename(tmp.data(), name.c_str()) != 0) ok = false;
    if (!ok) ::unlink(tmp.data());
    return ok;
}
} // namespace

void SetVM(JavaVM* vm) { g_vm.store(vm); }

std::string EnsureInstalled() {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto* vm = FindVM();
    if (!vm) return {};
    ThreadEnv thread(vm);
    JNIEnv* env = thread.env;
    if (!env || env->PushLocalFrame(24) != JNI_OK) return {};
    struct LocalFrame { JNIEnv* e; ~LocalFrame() { e->PopLocalFrame(nullptr); } } frame{env};

    const jclass activity_thread = env->FindClass("android/app/ActivityThread");
    const jclass context = env->FindClass("android/content/Context");
    const jclass file = env->FindClass("java/io/File");
    if (ClearException(env) || !activity_thread || !context || !file) return {};
    const jmethodID current = env->GetStaticMethodID(activity_thread, "currentApplication", "()Landroid/app/Application;");
    const jmethodID get_assets = env->GetMethodID(context, "getAssets", "()Landroid/content/res/AssetManager;");
    const jmethodID get_files = env->GetMethodID(context, "getFilesDir", "()Ljava/io/File;");
    const jmethodID get_path = env->GetMethodID(file, "getAbsolutePath", "()Ljava/lang/String;");
    if (ClearException(env) || !current || !get_assets || !get_files || !get_path) return {};
    const jobject app = env->CallStaticObjectMethod(activity_thread, current);
    if (ClearException(env) || !app) return {};
    const jobject asset_java = env->CallObjectMethod(app, get_assets);
    const jobject files_java = env->CallObjectMethod(app, get_files);
    if (ClearException(env) || !asset_java || !files_java) return {};
    const jstring path_java = static_cast<jstring>(env->CallObjectMethod(files_java, get_path));
    if (ClearException(env) || !path_java) return {};
    const char* chars = env->GetStringUTFChars(path_java, nullptr);
    if (ClearException(env) || !chars) return {};
    std::string dir(chars);
    env->ReleaseStringUTFChars(path_java, chars);
    if (dir.empty()) return {};
    dir += "/gmm_seda";
    AAssetManager* manager = AAssetManager_fromJava(env, asset_java);
    if (!manager) return {};
    const std::string version = ReadAssetText(manager, "gmm_seda/asset-version.txt");
    if (version.empty()) return {}; // This APK does not contain the asset package.
    const std::string names[] = {"tokens.txt", "c32/encoder.int8.onnx", "c32/decoder.onnx", "c32/joiner.int8.onnx"};
    auto all_valid = [&] {
        for (const auto& name : names) if (!ValidSize(manager, dir, name)) return false;
        return true;
    };
    if (ReadDiskText(dir + "/asset-version.txt") == version && all_valid()) return dir;
    if (!MakeDir(dir) || !MakeDir(dir + "/c32")) return {};
    for (const auto& name : names) {
        if (!CopyAsset(manager, name, dir + "/" + name)) {
            __android_log_print(ANDROID_LOG_ERROR, "multiplayermod", "GMM_SEDA_ASSETS copy failed: %s", name.c_str());
            return {};
        }
    }
    if (!all_valid() || !WriteVersion(dir + "/asset-version.txt", version)) return {};
    __android_log_print(ANDROID_LOG_INFO, "multiplayermod", "GMM_SEDA_ASSETS ready (APK assets -> internal files/gmm_seda)");
    return dir;
}
} // namespace seda_assets
