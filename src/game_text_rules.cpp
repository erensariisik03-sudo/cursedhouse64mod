#include "game_text_rules.h"
#include "username_gate.h"
#include "text_buffer.h"
#include "dobby.h"
#include <android/log.h>
#include <algorithm>
#include <array>
#include <cstring>
#if defined(__aarch64__)
#include <link.h>
#endif
#define RULE_LOG(...) __android_log_print(ANDROID_LOG_INFO, "multiplayermod", __VA_ARGS__)
namespace game_text_rules {
namespace {
using namespace il2cpp;
struct RootGuard {
    const Api& api; Handle root;
    ~RootGuard() { if (root) api.gchandle_free(root); }
};
bool StoreReference(const Api& api, Field* field, Object* object) {
    // IL2CPP Field::StaticSetValue uses deref_pointer=false for reference fields.
    // Pass the managed Object*, never the address of a native Object* variable.
    api.field_static_set_value(field, object);
    Object* check = nullptr;
    api.field_static_get_value(field, &check);
    return check == object;
}
bool ReplaceFilter(const Api& api, Class* klass, const char* field_name, Handle root) {
    if (!klass) return false;
    Field* field = api.class_get_field_from_name(klass, field_name);
    if (!field) return false;
    // Let the original .cctor complete, including its obfuscated pattern decoder.
    try { api.runtime_class_init(klass); } catch (...) { return false; }
    Object* replacement = api.gchandle_get_target(root);
    if (!replacement) return false;
    Object* previous = nullptr;
    api.field_static_get_value(field, &previous);
    if (!previous || api.object_get_class(previous) != api.object_get_class(replacement)) return false;
    const Handle backup = api.gchandle_new(previous, false);
    if (!backup) return false;
    RootGuard original_guard{api,backup};
    replacement = api.gchandle_get_target(root);
    if (!replacement) return false;
    if (StoreReference(api, field, replacement)) return true;
    Object* original = api.gchandle_get_target(backup);
    const bool restored = original && StoreReference(api, field, original);
    RULE_LOG("v13 filter write not verified; original-restored=%s", restored ? "yes" : "no");
    return false;
}
void Filters(const Api& api, Status& status) {
    if (!api.class_get_field_from_name || !api.field_static_get_value || !api.field_static_set_value ||
        !api.runtime_class_init || !api.object_new || !api.object_get_class) return;
    Class* regex = api.Find("System.Text.RegularExpressions", "Regex");
    const Method* constructor = api.FindMethod(regex, ".ctor", 1);
    if (!regex || !constructor) return;
    Object* object = api.object_new(regex);
    if (!object) return;
    const Handle root = api.gchandle_new(object, false);
    if (!root) return;
    RootGuard guard{api,root};
    String* pattern = api.string_new("(?!)"); // Negative empty lookahead: matches no text.
    if (pattern) {
        object = api.gchandle_get_target(root);
        if (!object) return;
        void* args[] = {pattern}; bool ok = false;
        api.Call(constructor, object, args, &ok);
        if (ok && api.gchandle_get_target(root)) {
            status.input_filter = ReplaceFilter(api, api.Find("", "TMPInputFieldTagFilter"), "qfy", root);
            status.display_filter = ReplaceFilter(api, api.Find("", "TMPTextTagFilter"), "qfz", root);
        }
    }
}
#if defined(__aarch64__)
Api g_limit_api{};
username_gate::Gate g_gate{};
struct Range { uintptr_t entry; size_t size = 0; };
int ExecutableRange(dl_phdr_info* image, size_t, void* opaque) {
    const char* name = image->dlpi_name ? std::strrchr(image->dlpi_name, '/') : nullptr;
    name = name ? name + 1 : image->dlpi_name;
    if (!name || std::strcmp(name, "libil2cpp.so") != 0) return 0;
    auto& range = *static_cast<Range*>(opaque);
    for (size_t i=0; i<image->dlpi_phnum; ++i) {
        const auto& segment = image->dlpi_phdr[i];
        if (segment.p_type != PT_LOAD || !(segment.p_flags & PF_X)) continue;
        const uintptr_t start = image->dlpi_addr + segment.p_vaddr;
        if (range.entry >= start && range.entry < start + segment.p_memsz) {
            range.size = start + segment.p_memsz - range.entry;
            return 1;
        }
    }
    return 0;
}
void LengthComparison(void*, DobbyRegisterContext* context) {
    if (!context) return;
    auto& length = context->general.x[g_gate.length_register];
    auto* string = reinterpret_cast<String*>(context->general.x[g_gate.string_register]);
    if (!string || length <= g_gate.old_limit) return;
    const int units = g_limit_api.string_length(string);
    if (units < 0 || static_cast<uint64_t>(units) != length) return;
    if (static_cast<size_t>(units) > text_buffer::kLimit * 2) return;
    const auto* chars = g_limit_api.string_chars(string);
    length = username_gate::ComparisonValue(static_cast<uint32_t>(units), chars, text_buffer::kLimit);
}
bool Username(const Api& api) {
    if (!api.class_get_nested_types) return false;
    Class* owner = api.Find("", "fm");
    if (!owner) return false;
    Class* state = nullptr; void* iterator = nullptr;
    while (Class* child = api.class_get_nested_types(owner, &iterator)) {
        const char* name = api.class_get_name(child);
        if (name && std::strcmp(name, "<RequestAsync>d__2") == 0) { state = child; break; }
    }
    const Method* move = api.FindMethod(state, "MoveNext", 0);
    const Method* next = api.FindMethod(state, "SetStateMachine", 1);
    const uintptr_t entry = reinterpret_cast<uintptr_t>(Entry(move));
    const uintptr_t end = reinterpret_cast<uintptr_t>(Entry(next));
    // Never scan a guessed RVA, unaligned instructions, an unrelated image, or past a known next method.
    if (!entry || (entry & 3) || end <= entry || end - entry > 65536) return false;
    Range range{entry,0}; dl_iterate_phdr(ExecutableRange, &range);
    const size_t bytes = std::min({static_cast<size_t>(end-entry),range.size,size_t{4096}});
    if (bytes < 24) return false;
    std::array<uint32_t,1024> code{};
    std::memcpy(code.data(), reinterpret_cast<void*>(entry), bytes - bytes % 4);
    const auto gate = username_gate::Find(code.data(),bytes/4);
    if (!gate) return false;
    g_limit_api = api; g_gate = *gate;
    // The original CMP and conditional branch execute after this callback. Only
    // their temporary length operand is adjusted; the actual String is untouched.
    const int result=DobbyInstrument(reinterpret_cast<void*>(entry + gate->compare * 4), LengthComparison);
    RULE_LOG("v13 username gate: %zu-bit compare+0x%zx string-reg=%u length-reg=%u instrument=%s",
             sizeof(void*)*8,gate->compare*4,gate->string_register,gate->length_register,result==0?"ready":"failed");
    return result == 0;
}
#else
bool Username(const Api&) { return false; }
#endif
}
Status Install(const Api& api) {
    Status status;
    try { Filters(api,status); } catch (...) { RULE_LOG("v13 tag rule setup failed; inspect rule status"); }
    try { status.username_limit = Username(api); } catch (...) { RULE_LOG("v13 username rule setup failed; original upper check retained"); }
    RULE_LOG("v13 text rules: input-filter=%s display-filter=%s username-limit=%s (1024 Unicode characters)",
             status.input_filter ? "disabled" : "unchanged", status.display_filter ? "disabled" : "unchanged",
             status.username_limit ? "enabled" : "unchanged; native pattern not verified");
    return status;
}
}
