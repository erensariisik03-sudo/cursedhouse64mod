#define MM_AUTO_UNITY 0
#include "../src/unity_keyboard.cpp"
#include <cassert>
#include <iostream>
namespace {
int hookCount=0;
void* observer=nullptr;
}
extern "C" int __android_log_print(int, const char*, const char*, ...) { return 0; }
extern "C" int DobbyHookNear(void*,void*,void**){return -1;}
extern "C" int DobbyHook(void* address, void* replacement, void** original) {
 ++hookCount; *original=address; if(hookCount==1) observer=replacement; return 0;
}
extern "C" EGLContext eglGetCurrentContext() { return nullptr; }
extern "C" EGLSurface eglGetCurrentSurface(EGLint) { return nullptr; }
extern "C" EGLBoolean eglQueryContext(EGLDisplay,EGLContext,EGLint,EGLint*) { return 0; }
extern "C" EGLBoolean eglQuerySurface(EGLDisplay,EGLSurface,EGLint,EGLint*) { return 0; }
extern "C" EGLBoolean eglSwapBuffers(EGLDisplay,EGLSurface) { return 1; }
namespace chat_bot_http {
void SetVM(JavaVM*) {}
bool Speak(const std::string&,std::string&) { return false; }
bool Post(const std::string&,const std::string&,const std::string&,int& code,std::string&) { code=0; return false; }
}
namespace keyboard_bridge {
void OpenNative(const char*,bool) {}
bool PollNative(char*,size_t,bool&,bool&) { return false; }
void CloseNative() {}
void SurfaceFrame(int,int) {}
bool RenderReady() { return false; }
void Open(const char*,bool) {}
void Close() {}
void ReconcileText(const char*,const char*) {}
bool ConsumeEmojiRequest() { return false; }
void SetEmojiSupport(uint64_t,bool) {}
void Snapshot(char*,size_t,bool&,bool&,bool&) {}
bool Pointer(float,float,bool) { return false; }
}
int main() {
 auto* library=dlopen("libil2cpp.so",RTLD_NOW|RTLD_GLOBAL); assert(library);
 auto domainCalls=reinterpret_cast<int(*)()>(dlsym(library,"fake_domain_calls"));
 auto lookupCalls=reinterpret_cast<int(*)()>(dlsym(library,"fake_metadata_lookups"));
 auto invocations=reinterpret_cast<int(*)()>(dlsym(library,"fake_invocations"));
 auto event=reinterpret_cast<const Method*(*)(int)>(dlsym(library,"fake_event"));
 auto setThrow=reinterpret_cast<void(*)(bool)>(dlsym(library,"fake_throw_next"));
 Bootstrap();
 assert(observer && hookCount==1 && !g_ready && !g_install_claimed);
 assert(domainCalls()==0 && lookupCalls()==0);
 auto invoke=reinterpret_cast<Invoke>(observer);
 Object* error=nullptr;
 // VM domain is already non-null in the fake runtime, but init has not finished.
 assert(invoke(event(0),reinterpret_cast<void*>(1),nullptr,&error)==reinterpret_cast<Object*>(0x1234));
 invoke(event(1),reinterpret_cast<void*>(1),nullptr,&error);
 invoke(event(2),nullptr,nullptr,&error);
 assert(!g_ready && domainCalls()==0 && lookupCalls()==0 && hookCount==1);
 setThrow(true); invoke(event(2),reinterpret_cast<void*>(1),nullptr,&error);
 assert(error && !g_ready && domainCalls()==0 && lookupCalls()==0);
 setThrow(false); invoke(event(2),reinterpret_cast<void*>(1),nullptr,&error);
 assert(!error && g_ready && g_install_claimed && domainCalls()>0 && lookupCalls()>0 && hookCount==9);
 invoke(event(2),reinterpret_cast<void*>(1),nullptr,&error);
 assert(hookCount==9 && invocations()==6); // one initialization, no recursive invoke
 std::cout << "Startup regression: no VM access before Unity frame, original result/exception forwarding, one hook installation: PASS\n";
}
