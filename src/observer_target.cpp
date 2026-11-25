#include "observer_target.h"
#include "hook_veneer.h"
#if defined(__aarch64__)
#include <link.h>
#include <dlfcn.h>
#include <cstring>
namespace {
struct Query { uintptr_t base, address; void* output; size_t bytes; bool copied=false; };
int ReadSegment(dl_phdr_info* image,size_t,void* opaque) {
    auto& q=*static_cast<Query*>(opaque);
    if (image->dlpi_addr!=q.base) return 0;
    for (size_t i=0;i<image->dlpi_phnum;++i) {
        const auto& p=image->dlpi_phdr[i];
        if (p.p_type!=PT_LOAD || (p.p_flags&(PF_R|PF_X))!=(PF_R|PF_X)) continue;
        const uintptr_t start=image->dlpi_addr+p.p_vaddr;
        if (q.address<start || q.address-start>p.p_memsz || q.bytes>p.p_memsz-(q.address-start)) continue;
        std::memcpy(q.output,reinterpret_cast<const void*>(q.address),q.bytes);q.copied=true;return 1;
    }
    return 1;
}
bool Read(uintptr_t address,void* output,size_t bytes,void* context) {
    Query query{*static_cast<uintptr_t*>(context),address,output,bytes,false};
    dl_iterate_phdr(ReadSegment,&query);return query.copied;
}
}
#endif
namespace observer_target {
void* Resolve(const il2cpp::Api& api,unsigned& hops) {
    hops=0;
    void* original=reinterpret_cast<void*>(api.runtime_invoke);
#if defined(__aarch64__)
    constexpr size_t footprint=16;
    Dl_info image{};
    const uintptr_t raw=reinterpret_cast<uintptr_t>(original);
    const uintptr_t entry=raw;
    if (!entry || !dladdr(reinterpret_cast<void*>(entry),&image) || !image.dli_fbase) return nullptr;
    uintptr_t base=reinterpret_cast<uintptr_t>(image.dli_fbase);
    const auto target=hook_veneer::Resolve(raw,Read,&base);
    if (!target) return nullptr;
    const uintptr_t normalized=target->address;
    uint8_t window[footprint];
    if (!Read(normalized,window,sizeof(window),&base)) return nullptr;
    // Never cover another exported API entry with the observer patch.
    const uintptr_t protected_entries[]={
#define API_ENTRY(name) reinterpret_cast<uintptr_t>(api.name)
        API_ENTRY(domain_get),API_ENTRY(domain_get_assemblies),API_ENTRY(assembly_get_image),
        API_ENTRY(class_from_name),API_ENTRY(class_get_method_from_name),API_ENTRY(class_get_parent),
        API_ENTRY(object_unbox),API_ENTRY(object_get_class),API_ENTRY(string_new),API_ENTRY(string_length),
        API_ENTRY(string_chars),API_ENTRY(gchandle_new),API_ENTRY(gchandle_get_target),API_ENTRY(gchandle_free),
        API_ENTRY(method_get_name),API_ENTRY(method_get_class),API_ENTRY(class_get_name),API_ENTRY(class_get_namespace),
        API_ENTRY(class_get_field_from_name),API_ENTRY(field_static_get_value),API_ENTRY(field_static_set_value),
        API_ENTRY(runtime_class_init),API_ENTRY(object_new),API_ENTRY(class_get_nested_types)
#undef API_ENTRY
    };
    for (uintptr_t symbol:protected_entries) if (hook_veneer::Overlaps(target->address,footprint,symbol)) return nullptr;
    hops=target->hops;return reinterpret_cast<void*>(target->address);
#else
    return original; // Host fake-runtime tests do not execute AArch64 machine code.
#endif
}
}
