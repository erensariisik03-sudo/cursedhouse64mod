#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace il2cpp {
// Pointer-width storage matches Unity 6 handles on the ARM64 target.
using Handle = uintptr_t;
static_assert(sizeof(Handle) == sizeof(void*));
struct Object; struct Class; struct Domain; struct Assembly; struct Image; struct Method; struct String; struct Field; struct Type;
struct Api {
    Domain* (*domain_get)();
    const Assembly** (*domain_get_assemblies)(Domain*, size_t*);
    const Image* (*assembly_get_image)(const Assembly*);
    Class* (*class_from_name)(const Image*, const char*, const char*);
    const Method* (*class_get_method_from_name)(Class*, const char*, int);
    Class* (*class_get_parent)(Class*);
    Object* (*runtime_invoke)(const Method*, void*, void**, Object**);
    void* (*object_unbox)(Object*);
    Class* (*object_get_class)(Object*); // optional, for live emoji font checks
    String* (*string_new)(const char*);
    int32_t (*string_length)(String*);
    const uint16_t* (*string_chars)(String*);
    Handle (*gchandle_new)(Object*, bool);
    Object* (*gchandle_get_target)(Handle);
    void (*gchandle_free)(Handle);
    const char* (*method_get_name)(const Method*);
    Class* (*method_get_class)(const Method*);
    const char* (*class_get_name)(Class*);
    const char* (*class_get_namespace)(Class*);
    // Optional APIs used only by the explicitly requested game text rules.
    Field* (*class_get_field_from_name)(Class*, const char*);
    void (*field_static_get_value)(Field*, void*);
    // Setter: Object* itself for reference fields; address of data for scalar fields.
    // Getter above always takes an output address. These are not symmetric.
    void (*field_static_set_value)(Field*, void*);
    void (*runtime_class_init)(Class*);
    Object* (*object_new)(Class*);
    Class* (*class_get_nested_types)(Class*, void**);
    // Optional, read-only discovery for the game cosmetic catalogue.
    void (*field_get_value)(Object*, Field*, void*);
    void (*field_set_value)(Object*, Field*, void*); // optional, verified scoped/scalar writes
    const Type* (*class_get_type)(Class*);
    Object* (*type_get_object)(const Type*);
    const Type* (*field_get_type)(Field*);
    Class* (*class_from_type)(const Type*);
    const Method* (*class_get_methods)(Class*,void**); // optional exact-overload discovery
    uint32_t (*method_get_param_count)(const Method*);
    const Type* (*method_get_param)(const Method*,uint32_t);
    Object* (*array_new)(Class*,uintptr_t); // optional, normal SDK RPC argument arrays
    bool (*method_is_generic)(const Method*); // optional, exclude generic overload definitions
    void (*field_set_value_object)(Object*,Field*,Object*); // optional managed reference write with GC barrier
    int32_t (*class_value_size)(Class*,uint32_t*); // optional value-type ABI validation
    Field* (*class_get_fields)(Class*,void**); // optional clone reference repair
    const char* (*field_get_name)(Field*);
    uint32_t (*field_get_flags)(Field*);
    uintptr_t (*array_length)(Object*);
    uint32_t (*array_object_header_size)();
    int32_t (*array_element_size)(Class*);
    Class* (*class_get_element_class)(Class*);
    bool Load(void* library);
    Class* Find(const char* namespc, const char* name) const;
    const Method* FindMethod(Class* klass, const char* name, int count) const;
    Object* Call(const Method* method, Object* self = nullptr, void** args = nullptr, bool* ok = nullptr) const;
    template<class T> T Value(const Method* method, Object* self = nullptr, T fallback = T{}) const {
        auto* boxed = Call(method, self);
        return boxed ? *static_cast<T*>(object_unbox(boxed)) : fallback;
    }
    std::string Utf8(String* text) const;
};
// Standard IL2CPP MethodInfo begins with methodPointer on the supported Unity ABI.
// Do not infer an address from dump.cs RVAs; resolve metadata on the running build.
void* Entry(const Method* method);
}
