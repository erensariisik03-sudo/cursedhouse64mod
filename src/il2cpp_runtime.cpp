#include "il2cpp_runtime.h"
#include <dlfcn.h>
#include <cstring>
#include <android/log.h>
namespace il2cpp {
bool Api::Load(void* library) {
#define LOAD(name) name = reinterpret_cast<decltype(name)>(dlsym(library, "il2cpp_" #name)); if (!name) { __android_log_print(ANDROID_LOG_ERROR, "multiplayermod", "Missing export: il2cpp_%s", #name); return false; }
    LOAD(domain_get) LOAD(domain_get_assemblies) LOAD(assembly_get_image)
    LOAD(class_from_name) LOAD(class_get_method_from_name) LOAD(class_get_parent)
    LOAD(runtime_invoke) LOAD(object_unbox) LOAD(string_new) LOAD(string_length) LOAD(string_chars)
    LOAD(gchandle_new) LOAD(gchandle_get_target) LOAD(gchandle_free)
    LOAD(method_get_name) LOAD(method_get_class) LOAD(class_get_name) LOAD(class_get_namespace)
#undef LOAD
    object_get_class = reinterpret_cast<decltype(object_get_class)>(dlsym(library, "il2cpp_object_get_class"));
#define OPTIONAL(name) name = reinterpret_cast<decltype(name)>(dlsym(library, "il2cpp_" #name));
    OPTIONAL(class_get_field_from_name) OPTIONAL(field_static_get_value)
    OPTIONAL(field_static_set_value) OPTIONAL(runtime_class_init) OPTIONAL(object_new)
    OPTIONAL(class_get_nested_types)
    OPTIONAL(field_get_value) OPTIONAL(class_get_type) OPTIONAL(type_get_object)
    OPTIONAL(field_get_type) OPTIONAL(class_from_type)
#undef OPTIONAL
    return true;
}
Class* Api::Find(const char* namespc, const char* name) const {
    Domain* domain = domain_get(); if (!domain) return nullptr;
    size_t count = 0; auto assemblies = domain_get_assemblies(domain, &count);
    if (!assemblies) return nullptr;
    for (size_t i = 0; i < count; ++i)
        if (auto* klass = class_from_name(assembly_get_image(assemblies[i]), namespc, name)) return klass;
    return nullptr;
}
const Method* Api::FindMethod(Class* klass, const char* name, int count) const {
    while (klass) {
        if (auto* method = class_get_method_from_name(klass, name, count)) return method;
        klass = class_get_parent(klass);
    }
    return nullptr;
}
Object* Api::Call(const Method* method, Object* self, void** args, bool* ok) const {
    if (ok) *ok = false;
    if (!method) return nullptr;
    Object* error = nullptr;
    Object* result = runtime_invoke(method, self, args, &error);
    if (error) {
        // Never stringify managed exceptions: their messages can contain field text.
        static thread_local int count = 0;
        if (++count <= 4) __android_log_print(ANDROID_LOG_ERROR, "multiplayermod", "Managed bridge invocation failed (text omitted)");
        return nullptr;
    }
    if (ok) *ok = true;
    return result;
}
std::string Api::Utf8(String* text) const {
    std::string out; if (!text) return out;
    const auto* chars = string_chars(text); const int n = string_length(text);
    for (int i = 0; i < n; ++i) {
        uint32_t cp = chars[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < n && chars[i+1] >= 0xDC00 && chars[i+1] <= 0xDFFF)
            cp = 0x10000 + ((cp - 0xD800) << 10) + chars[++i] - 0xDC00;
        else if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
        if (cp == 0) continue;
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 63)); }
        else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
        else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 63)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
    }
    return out;
}
void* Entry(const Method* method) {
    void* pointer = nullptr;
    if (method) std::memcpy(&pointer, method, sizeof(pointer));
    return pointer;
}
}
