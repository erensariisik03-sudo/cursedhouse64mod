#include "il2cpp_runtime.h"
#include <cstdlib>
#include <cstring>
#include <deque>
namespace il2cpp {
struct Object {}; struct String {}; struct Domain {}; struct Assembly {}; struct Image {};
struct Class { const char* ns; const char* name; };
struct Method { void* code; const char* name; Class* klass; };
}
using namespace il2cpp;
namespace {
Domain domain; Assembly assembly; Image image;
const Assembly* assemblies[] = {&assembly};
Class classes[] = {{"TMPro","TMP_InputField"},{"UnityEngine.EventSystems","EventSystem"},
 {"UnityEngine","Screen"},{"UnityEngine","Object"},{"UnityEngine.InputSystem","Touchscreen"},
 {"UnityEngine.InputSystem.Controls","TouchControl"},{"UnityEngine.InputSystem.Controls","ButtonControl"},
 {"UnityEngine.InputSystem","InputControlExtensions"},{"UnityEngine","Input"},{"System","Other"}};
std::deque<Method> methods;
int domains = 0, lookups = 0, invocations = 0;
bool throwNext = false;
void DummyMethod() {}
}
extern "C" {
Domain* il2cpp_domain_get() { ++domains; return &domain; }
const Assembly** il2cpp_domain_get_assemblies(Domain*, size_t* count) { ++lookups; *count=1; return assemblies; }
const Image* il2cpp_assembly_get_image(const Assembly*) { return &image; }
Class* il2cpp_class_from_name(const Image*, const char* ns, const char* name) {
 ++lookups;
 for(auto& c:classes) if (!std::strcmp(c.ns,ns) && !std::strcmp(c.name,name)) return &c;
 return nullptr;
}
const Method* il2cpp_class_get_method_from_name(Class* c, const char* name, int) {
 ++lookups; methods.push_back({reinterpret_cast<void*>(DummyMethod),name,c}); return &methods.back();
}
Class* il2cpp_class_get_parent(Class*) { return nullptr; }
const char* il2cpp_method_get_name(const Method* m) { return m->name; }
Class* il2cpp_method_get_class(const Method* m) { return m->klass; }
const char* il2cpp_class_get_name(Class* c) { return c->name; }
const char* il2cpp_class_get_namespace(Class* c) { return c->ns; }
Object* il2cpp_runtime_invoke(const Method*, void*, void**, Object** error) {
 ++invocations; if(error) *error = throwNext ? reinterpret_cast<Object*>(0x10) : nullptr;
 return reinterpret_cast<Object*>(0x1234);
}
void* il2cpp_object_unbox(Object* object) { return object; }
String* il2cpp_string_new(const char*) { return nullptr; }
int32_t il2cpp_string_length(String*) { return 0; }
const uint16_t* il2cpp_string_chars(String*) { static const uint16_t empty[]={0}; return empty; }
Handle il2cpp_gchandle_new(Object*, bool) { return 1; }
Object* il2cpp_gchandle_get_target(Handle) { return nullptr; }
void il2cpp_gchandle_free(Handle) {}
// Any background attach reproduces the old failure deterministically.
void* il2cpp_thread_attach(Domain*) { std::abort(); }
void il2cpp_thread_detach(void*) { std::abort(); }
int fake_domain_calls() { return domains; }
int fake_metadata_lookups() { return lookups; }
int fake_invocations() { return invocations; }
void fake_throw_next(bool value) { throwNext=value; }
const Method* fake_event(int kind) {
 static Method boot{reinterpret_cast<void*>(DummyMethod),".cctor",&classes[1]};
 static Method unrelated{reinterpret_cast<void*>(DummyMethod),"Update",&classes[9]};
 static Method update{reinterpret_cast<void*>(DummyMethod),"Update",&classes[1]};
 return kind==0?&boot:kind==1?&unrelated:&update;
}
}
