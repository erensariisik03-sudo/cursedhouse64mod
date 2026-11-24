#include "storage_access.h"
#ifdef __ANDROID__
#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <string>
#include <initializer_list>
#include <atomic>
#include <algorithm>

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
bool ShowToast(const char* message){
    if(!message||!*message)return false;
    Env t;JNIEnv* env=t.env;
    if(!env||env->PushLocalFrame(30)!=JNI_OK)return false;
    bool ok=false;
    do{
        jclass at=env->FindClass("android/app/ActivityThread");
        jclass toast=env->FindClass("android/widget/Toast");
        if(!at||!toast||Failed(env))break;
        jmethodID appMethod=env->GetStaticMethodID(at,"currentApplication","()Landroid/app/Application;");
        jmethodID make=env->GetStaticMethodID(toast,"makeText","(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
        jmethodID show=env->GetMethodID(toast,"show","()V");
        if(!appMethod||!make||!show||Failed(env))break;
        jobject app=env->CallStaticObjectMethod(at,appMethod);
        jstring str=env->NewStringUTF(message);
        if(!app||!str||Failed(env))break;
        jobject instance=env->CallStaticObjectMethod(toast,make,app,str,static_cast<jint>(1));
        if(!instance||Failed(env))break;
        env->CallVoidMethod(instance,show);
        ok=!Failed(env);
    }while(false);
    Failed(env);env->PopLocalFrame(nullptr);return ok;
}

// Uses Android MediaStore instead of requiring raw /sdcard directory writes.
// The provider grants the inserting app access to its own item even when
// MANAGE_EXTERNAL_STORAGE was not granted. The receiver still had to approve
// the source owner's sharing session to get these bytes.
bool SaveImageToGallery(const uint8_t* data,size_t size,bool png,uint64_t id,std::string& path){
    path.clear();
    if(!data||size<8||size>8*1024*1024)return false;
    Env t;JNIEnv* env=t.env;
    if(!env||env->PushLocalFrame(64)!=JNI_OK)return false;
    bool ok=false;
    jobject uri=nullptr,resolver=nullptr;
    do {
        if(Sdk(env)<29)break;
        jclass at=env->FindClass("android/app/ActivityThread");
        jclass context=env->FindClass("android/content/Context");
        jclass values=env->FindClass("android/content/ContentValues");
        jclass media=env->FindClass("android/provider/MediaStore$Images$Media");
        jclass cr=env->FindClass("android/content/ContentResolver");
        jclass stream=env->FindClass("java/io/OutputStream");
        if(!at||!context||!values||!media||!cr||!stream||Failed(env))break;
        auto appMethod=env->GetStaticMethodID(at,"currentApplication","()Landroid/app/Application;");
        auto getResolver=env->GetMethodID(context,"getContentResolver","()Landroid/content/ContentResolver;");
        auto contentCtor=env->GetMethodID(values,"<init>","()V");
        auto putString=env->GetMethodID(values,"put","(Ljava/lang/String;Ljava/lang/String;)V");
        auto putInt=env->GetMethodID(values,"put","(Ljava/lang/String;Ljava/lang/Integer;)V");
        auto integerClass=env->FindClass("java/lang/Integer");
        auto integerValue=integerClass?env->GetStaticMethodID(integerClass,"valueOf","(I)Ljava/lang/Integer;"):nullptr;
        auto external=env->GetStaticFieldID(media,"EXTERNAL_CONTENT_URI","Landroid/net/Uri;");
        auto insert=env->GetMethodID(cr,"insert","(Landroid/net/Uri;Landroid/content/ContentValues;)Landroid/net/Uri;");
        auto open=env->GetMethodID(cr,"openOutputStream","(Landroid/net/Uri;)Ljava/io/OutputStream;");
        auto update=env->GetMethodID(cr,"update","(Landroid/net/Uri;Landroid/content/ContentValues;Ljava/lang/String;[Ljava/lang/String;)I");
        auto remove=env->GetMethodID(cr,"delete","(Landroid/net/Uri;Ljava/lang/String;[Ljava/lang/String;)I");
        auto write=env->GetMethodID(stream,"write","([BII)V");
        auto close=env->GetMethodID(stream,"close","()V");
        if(!appMethod||!getResolver||!contentCtor||!putString||!putInt||!integerValue||!external||!insert||!open||!update||!remove||!write||!close||Failed(env))break;
        jobject app=env->CallStaticObjectMethod(at,appMethod);
        if(!app||Failed(env))break;
        resolver=env->CallObjectMethod(app,getResolver);
        jobject collection=env->GetStaticObjectField(media,external);
        jobject cv=env->NewObject(values,contentCtor);
        if(!resolver||!collection||!cv||Failed(env))break;
        // Restrict keys to fixed MediaStore columns; no remote filename/path.
        const std::string fname="received_"+std::to_string(id)+"_"+
                                  std::to_string(static_cast<unsigned long long>(size))+(png?".png":".jpg");
        auto put=[&](const char* k,const char* v){
            jstring key=env->NewStringUTF(k),value=env->NewStringUTF(v);
            if(!key||!value||Failed(env))return false;
            env->CallVoidMethod(cv,putString,key,value);
            env->DeleteLocalRef(key);env->DeleteLocalRef(value);
            return !Failed(env);
        };
        if(!put("_display_name",fname.c_str())||!put("mime_type",png?"image/png":"image/jpeg")||
           !put("relative_path","Pictures/GMMReceived/"))break;
        jstring pendingKey=env->NewStringUTF("is_pending");
        jobject pendingVal=env->CallStaticObjectMethod(integerClass,integerValue,static_cast<jint>(1));
        if(!pendingKey||!pendingVal||Failed(env))break;
        env->CallVoidMethod(cv,putInt,pendingKey,pendingVal);
        if(Failed(env))break;
        uri=env->CallObjectMethod(resolver,insert,collection,cv);
        if(!uri||Failed(env))break;
        jobject os=env->CallObjectMethod(resolver,open,uri);
        if(!os||Failed(env))break;
        bool written=true;
        // Stream in bounded chunks to avoid a large temporary Java byte array.
        jbyteArray buf=env->NewByteArray(32768);
        if(!buf||Failed(env))written=false;
        for(size_t offset=0;written&&offset<size;){
            const jint length=static_cast<jint>(std::min(size-offset,size_t(32768)));
            env->SetByteArrayRegion(buf,0,length,reinterpret_cast<const jbyte*>(data+offset));
            if(Failed(env)){written=false;break;}
            env->CallVoidMethod(os,write,buf,static_cast<jint>(0),length);
            if(Failed(env)){written=false;break;}
            offset+=static_cast<size_t>(length);
        }
        env->CallVoidMethod(os,close);
        if(Failed(env))written=false;
        if(!written)break;
        jobject published=env->NewObject(values,contentCtor);
        jobject zero=env->CallStaticObjectMethod(integerClass,integerValue,static_cast<jint>(0));
        if(!published||!zero||Failed(env))break;
        env->CallVoidMethod(published,putInt,pendingKey,zero);
        if(Failed(env))break;
        env->CallIntMethod(resolver,update,uri,published,nullptr,nullptr);
        if(Failed(env))break;
        path="Pictures/GMMReceived/"+fname;
        ok=true;
    }while(false);
    if(!ok&&uri&&resolver){
        Failed(env);
        jclass cr=env->FindClass("android/content/ContentResolver");
        jmethodID remove=cr?env->GetMethodID(cr,"delete","(Landroid/net/Uri;Ljava/lang/String;[Ljava/lang/String;)I"):nullptr;
        if(remove&&!Failed(env))env->CallIntMethod(resolver,remove,uri,nullptr,nullptr);
    }
    Failed(env);
    env->PopLocalFrame(nullptr);
    return ok;
}

} // namespace storage_access
#endif
