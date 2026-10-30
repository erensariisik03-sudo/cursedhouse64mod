#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <time.h>
#include <unwind.h>
#include <atomic>
#include <vector>
#include <string>
#include "arm_hook.h"

#define LOG_TAG "CursedHouseChat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================
// 32-BIT RVA / FIELD DEFINITIONS - current dump.cs
// ============================================================

static constexpr uintptr_t kTMP_CharacterLimit_FieldOffset = 0x114;
static constexpr uintptr_t kTMP_SoftKeyboard_FieldOffset = 0x0B4;
static constexpr uintptr_t kTMP_ContentType_FieldOffset = 0x0F0;
static constexpr uintptr_t kTMP_CharacterValidation_FieldOffset = 0x108;
static constexpr uintptr_t kTMP_OnValidateInput_FieldOffset = 0x138;
static constexpr uintptr_t kTMP_Text_FieldOffset = 0x160;
static constexpr uintptr_t kTMP_RichText_FieldOffset = 0x16D;
static constexpr uintptr_t kTMP_IsRichTextEditingAllowed_FieldOffset = 0x1E6;
static constexpr uintptr_t kTMP_InputValidator_FieldOffset = 0x1F0;

// TMPro.TMP_InputField
static constexpr uintptr_t kTMP_SetCharacterLimit_RVA = 0x4089A20;
static constexpr uintptr_t kTMP_AppendString_RVA = 0x4093D2C;
static constexpr uintptr_t kTMP_AppendChar_RVA = 0x4093DD4;
static constexpr uintptr_t kTMP_InsertChar_RVA = 0x4094248;
static constexpr uintptr_t kTMP_ActivateInputFieldInternal_RVA = 0x408DCE4;
static constexpr uintptr_t kTMP_UpdateStringPositionFromKeyboard_RVA = 0x408D108;
static constexpr uintptr_t kTMP_Validate_RVA = 0x408E298;
static constexpr uintptr_t kTMP_IsValidChar_RVA = 0x4091B54;
static constexpr uintptr_t kTMP_SetOnValidateInput_RVA = 0x40899BC;
static constexpr uintptr_t kTMP_SetCharacterValidation_RVA = 0x408A2E4;
static constexpr uintptr_t kTMP_SetInputValidator_RVA = 0x408A364;
static constexpr uintptr_t kTMP_SetIsRichTextEditingAllowed_RVA = 0x4089E98;
static constexpr uintptr_t kTMP_SetRichText_RVA = 0x408A408;

// UnityEngine.TouchScreenKeyboard
static constexpr uintptr_t kTSK_GetText_RVA = 0x41B9614;
static constexpr uintptr_t kTSK_SetCharacterLimit_RVA = 0x41B9C4C;

// ============================================================
// FUNCTION POINTER TYPES
// ARM32 IL2CPP instance methods in this dump use a trailing
// MethodInfo pointer, matching the existing working hook setup.
// Validate returns a UTF-16 code unit (Il2CppChar/char).
// ============================================================

typedef void (*VoidInstanceFn)(void* instance, void* methodInfo);
typedef void (*IntInstanceFn)(void* instance, int value, void* methodInfo);
typedef void (*StringInstanceFn)(void* instance, void* stringObject, void* methodInfo);
typedef void (*ObjectInstanceFn)(void* instance, void* objectValue, void* methodInfo);
typedef void (*CharInstanceFn)(void* instance, uint32_t ch, void* methodInfo);
typedef uint16_t (*TMPValidateFn)(void* instance, void* textObject, int pos,
                                  uint16_t ch, void* methodInfo);
typedef bool (*TMPIsValidCharFn)(void* instance, uint16_t ch, void* methodInfo);

static VoidInstanceFn orig_TMP_ActivateInputFieldInternal = nullptr;
static VoidInstanceFn orig_TMP_UpdateStringPositionFromKeyboard = nullptr;
static IntInstanceFn orig_TMP_SetCharacterLimit = nullptr;
static IntInstanceFn orig_TSK_SetCharacterLimit = nullptr;
static StringInstanceFn orig_TMP_AppendString = nullptr;
static CharInstanceFn orig_TMP_AppendChar = nullptr;
static CharInstanceFn orig_TMP_InsertChar = nullptr;
static TMPValidateFn orig_TMP_Validate = nullptr;
static TMPIsValidCharFn orig_TMP_IsValidChar = nullptr;
static ObjectInstanceFn orig_TMP_SetOnValidateInput = nullptr;
static IntInstanceFn orig_TMP_SetCharacterValidation = nullptr;
static ObjectInstanceFn orig_TMP_SetInputValidator = nullptr;
static IntInstanceFn orig_TMP_SetIsRichTextEditingAllowed = nullptr;
static IntInstanceFn orig_TMP_SetRichText = nullptr;

static uintptr_t g_il2cppLoadBias = 0;

// ============================================================
// MEMORY / TEXT HELPERS
// ============================================================

template <typename T>
static T ReadField(void* instance, uintptr_t offset) {
    T value{};
    if (instance) {
        memcpy(&value, reinterpret_cast<const void*>(
            reinterpret_cast<uintptr_t>(instance) + offset), sizeof(T));
    }
    return value;
}

static bool IsInterestingChar(uint16_t ch) {
    if (ch == '<' || ch == '>' || ch == '=' || ch == '/') return true;
    if (ch == 's' || ch == 'S' || ch == 'i' || ch == 'I' ||
        ch == 'z' || ch == 'Z' || ch == 'e' || ch == 'E') return true;
    return false;
}

// IL2CPP string layout for this 32-bit build:
// Il2CppObject header (8 bytes), int32 length, UTF-16 chars.
// Reads only known string-object pointers supplied by TMP methods/fields.
static std::string ReadIl2CppStringAscii(void* stringObject, size_t maxChars = 120) {
    if (!stringObject) return "<null>";

    int32_t length = -1;
    const uintptr_t base = reinterpret_cast<uintptr_t>(stringObject);
    memcpy(&length, reinterpret_cast<const void*>(base + 8), sizeof(length));

    if (length < 0 || length > 4096) return "<invalid-string-length>";

    const size_t count = (static_cast<size_t>(length) < maxChars)
        ? static_cast<size_t>(length) : maxChars;

    std::string result;
    result.reserve(count + 24);

    const uint16_t* chars = reinterpret_cast<const uint16_t*>(base + 12);
    for (size_t i = 0; i < count; ++i) {
        uint16_t c = 0;
        memcpy(&c, chars + i, sizeof(c));
        if (c >= 32 && c <= 126) {
            if (c == '\\' || c == '"') result.push_back('\\');
            result.push_back(static_cast<char>(c));
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\r') {
            result += "\\r";
        } else if (c == '\t') {
            result += "\\t";
        } else {
            result.push_back('?');
        }
    }

    if (static_cast<size_t>(length) > count) result += "...";
    return result;
}

static void LogTMPInputState(void* instance, const char* reason) {
    if (!instance) {
        LOGW("TMP STATE: instance=NULL reason=%s", reason ? reason : "?");
        return;
    }

    const int limit = ReadField<int>(instance, kTMP_CharacterLimit_FieldOffset);
    const int contentType = ReadField<int>(instance, kTMP_ContentType_FieldOffset);
    const int validation = ReadField<int>(instance, kTMP_CharacterValidation_FieldOffset);
    const uintptr_t keyboard = ReadField<uintptr_t>(instance, kTMP_SoftKeyboard_FieldOffset);
    const uintptr_t onValidate = ReadField<uintptr_t>(instance, kTMP_OnValidateInput_FieldOffset);
    const uintptr_t validator = ReadField<uintptr_t>(instance, kTMP_InputValidator_FieldOffset);
    const uintptr_t textObject = ReadField<uintptr_t>(instance, kTMP_Text_FieldOffset);
    const uint8_t richText = ReadField<uint8_t>(instance, kTMP_RichText_FieldOffset);
    const uint8_t richEditing = ReadField<uint8_t>(instance, kTMP_IsRichTextEditingAllowed_FieldOffset);

    LOGI("TMP STATE [%s] field=%p limit=%d contentType=%d validation=%d",
         reason ? reason : "?", instance, limit, contentType, validation);
    LOGI("TMP STATE [%s] softKeyboard=%p onValidateInput=%p inputValidator=%p",
         reason ? reason : "?", reinterpret_cast<void*>(keyboard),
         reinterpret_cast<void*>(onValidate), reinterpret_cast<void*>(validator));
    LOGI("TMP STATE [%s] richText=%u richTextEditingAllowed=%u textObject=%p text=\"%s\"",
         reason ? reason : "?", static_cast<unsigned>(richText),
         static_cast<unsigned>(richEditing), reinterpret_cast<void*>(textObject),
         ReadIl2CppStringAscii(reinterpret_cast<void*>(textObject)).c_str());
}

// ============================================================
// /proc/self/maps HELPERS
// ============================================================

struct MapEntry {
    uintptr_t start = 0;
    uintptr_t end = 0;
    uintptr_t offset = 0;
    char perms[5] = {0};
    char path[512] = {0};
};

static bool ReadMaps(std::vector<MapEntry>& out) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return false;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        MapEntry e{};
        unsigned int devMajor = 0, devMinor = 0;
        unsigned long inode = 0;
        char pathname[512] = {0};
        const int n = sscanf(line,
            "%" SCNxPTR "-" "%" SCNxPTR " %4s %" SCNxPTR " %x:%x %lu %511[^\n]",
            &e.start, &e.end, e.perms, &e.offset,
            &devMajor, &devMinor, &inode, pathname);

        if (n >= 7) {
            if (n >= 8) {
                strncpy(e.path, pathname, sizeof(e.path) - 1);
                char* p = e.path;
                while (*p == ' ') ++p;
                if (p != e.path) memmove(e.path, p, strlen(p) + 1);
            }
            out.push_back(e);
        }
    }

    fclose(f);
    return true;
}

static uintptr_t GetModuleLoadBias(const char* moduleName) {
    std::vector<MapEntry> maps;
    if (!ReadMaps(maps)) return 0;

    uintptr_t best = 0;
    uintptr_t minBias = UINTPTR_MAX;
    bool found = false;

    for (const auto& e : maps) {
        if (strstr(e.path, moduleName) == nullptr) continue;
        found = true;

        const uintptr_t bias = e.start - e.offset;
        if (bias < minBias) minBias = bias;
        if (e.offset == 0) best = e.start;
    }

    if (!found) return 0;
    if (best != 0) return best;
    return (minBias == UINTPTR_MAX) ? 0 : minBias;
}

static inline uintptr_t RvaToFunctionAddress(uintptr_t rva) {
    if (!g_il2cppLoadBias || !rva) return 0;
    return g_il2cppLoadBias + rva;
}

static bool InstallArmHook(uintptr_t rva, void* replacement,
                           void** original, const char* label) {
    const uintptr_t target = RvaToFunctionAddress(rva);
    if (target == 0) {
        LOGE("Adres 0: %s RVA=0x%" PRIxPTR, label, rva);
        return false;
    }

    const int rc = ArmHook(reinterpret_cast<void*>(target), replacement, original);
    if (rc == 0 && original && *original) {
        LOGI("Kanca BASARILI: %s (RVA=0x%" PRIxPTR ", target=%p)",
             label, rva, reinterpret_cast<void*>(target));
        return true;
    }

    LOGE("Kanca BASARISIZ: %s (RVA=0x%" PRIxPTR ", hata=%d, original=%p)",
         label, rva, rc, original ? *original : nullptr);
    return false;
}

// ============================================================
// CHARACTER LIMIT HELPERS - existing behaviour retained
// ============================================================

static void ForceTMPObjectLimit(void* tmpInstance, const char* reason) {
    if (!tmpInstance) return;

    const uintptr_t address = reinterpret_cast<uintptr_t>(tmpInstance)
                            + kTMP_CharacterLimit_FieldOffset;
    const int oldValue = ReadField<int>(tmpInstance, kTMP_CharacterLimit_FieldOffset);

    // TMP_InputField uses 0 for no character limit.
    if (oldValue != 0 && oldValue != -1) {
        const int newValue = 0;
        memcpy(reinterpret_cast<void*>(address), &newValue, sizeof(newValue));
        LOGI("TMP LIMIT SIFIRLANDI: field=%p sebep=%s eski=%d yeni=0",
             tmpInstance, reason, oldValue);
    }
}

// ============================================================
// EXISTING TMP HOOKS
// ============================================================

static void my_TMP_ActivateInputFieldInternal(void* instance, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::ActivateInputFieldInternal field=%p", instance);
    LogTMPInputState(instance, "aktivasyon oncesi");
    ForceTMPObjectLimit(instance, "ActivateInputFieldInternal");

    if (orig_TMP_ActivateInputFieldInternal)
        orig_TMP_ActivateInputFieldInternal(instance, methodInfo);

    ForceTMPObjectLimit(instance, "ActivateInputFieldInternal sonrasi");
    LogTMPInputState(instance, "aktivasyon sonrasi");
}

static void my_TMP_AppendChar(void* instance, uint32_t ch, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::Append(char) field=%p char=0x%04" PRIX32,
         instance, ch);
    ForceTMPObjectLimit(instance, "Append(char) oncesi");

    if (orig_TMP_AppendChar)
        orig_TMP_AppendChar(instance, ch, methodInfo);

    ForceTMPObjectLimit(instance, "Append(char) sonrasi");
}

static void my_TMP_InsertChar(void* instance, uint32_t ch, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::Insert(char) field=%p char=0x%04" PRIX32,
         instance, ch);
    ForceTMPObjectLimit(instance, "Insert(char) oncesi");

    if (orig_TMP_InsertChar)
        orig_TMP_InsertChar(instance, ch, methodInfo);

    ForceTMPObjectLimit(instance, "Insert(char) sonrasi");
}

static void my_TMP_AppendString(void* instance, void* stringObject, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::Append(string) field=%p value=\"%s\"",
         instance, ReadIl2CppStringAscii(stringObject).c_str());
    ForceTMPObjectLimit(instance, "Append(string) oncesi");

    if (orig_TMP_AppendString)
        orig_TMP_AppendString(instance, stringObject, methodInfo);

    ForceTMPObjectLimit(instance, "Append(string) sonrasi");
}

static void my_TMP_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::set_characterLimit field=%p istenen=%d -> 0",
         instance, value);

    if (orig_TMP_SetCharacterLimit)
        orig_TMP_SetCharacterLimit(instance, 0, methodInfo);

    ForceTMPObjectLimit(instance, "set_characterLimit");
}

static void my_TSK_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("TETIKLENDI: TouchScreenKeyboard::set_characterLimit keyboard=%p istenen=%d -> 0",
         instance, value);
    if (orig_TSK_SetCharacterLimit)
        orig_TSK_SetCharacterLimit(instance, 0, methodInfo);
}

// ============================================================
// NEW DIAGNOSTIC HOOKS
// ============================================================

// TMP_InputField::Validate(string text, int pos, char ch) -> char
static uint16_t my_TMP_Validate(void* instance, void* textObject, int pos,
                                uint16_t ch, void* methodInfo) {
    uint16_t result = ch;
    if (orig_TMP_Validate)
        result = orig_TMP_Validate(instance, textObject, pos, ch, methodInfo);

    if (IsInterestingChar(ch) || result != ch) {
        LOGI("VALIDATE field=%p pos=%d input=0x%04X result=0x%04X text=\"%s\" %s",
             instance, pos, static_cast<unsigned>(ch),
             static_cast<unsigned>(result),
             ReadIl2CppStringAscii(textObject, 80).c_str(),
             (result == 0) ? "[REJECT/NULL]" : "[ACCEPT/RETURN]");
    }
    return result;
}

// TMP_InputField::IsValidChar(char) -> bool
static bool my_TMP_IsValidChar(void* instance, uint16_t ch, void* methodInfo) {
    const bool result = orig_TMP_IsValidChar
        ? orig_TMP_IsValidChar(instance, ch, methodInfo)
        : true;

    if (IsInterestingChar(ch) || !result) {
        LOGI("ISVALIDCHAR field=%p char=0x%04X allowed=%d",
             instance, static_cast<unsigned>(ch), result ? 1 : 0);
    }
    return result;
}

// TMP_InputField::UpdateStringPositionFromKeyboard() -> void
static void my_TMP_UpdateStringPositionFromKeyboard(void* instance, void* methodInfo) {
    if (orig_TMP_UpdateStringPositionFromKeyboard)
        orig_TMP_UpdateStringPositionFromKeyboard(instance, methodInfo);

    if (!instance) return;

    const uintptr_t textObject = ReadField<uintptr_t>(instance, kTMP_Text_FieldOffset);
    const std::string currentText = ReadIl2CppStringAscii(
        reinterpret_cast<void*>(textObject), 120);

    // This method may be called repeatedly. Print only when text changes.
    static void* lastInstance = nullptr;
    static std::string lastText;
    if (lastInstance != instance || lastText != currentText) {
        LOGI("KEYBOARD->TMP field=%p softKeyboard=%p textObject=%p text=\"%s\"",
             instance,
             reinterpret_cast<void*>(ReadField<uintptr_t>(instance, kTMP_SoftKeyboard_FieldOffset)),
             reinterpret_cast<void*>(textObject), currentText.c_str());
        lastInstance = instance;
        lastText = currentText;
    }
}

// Property setters are observed so we can see whether the game assigns
// a custom validator or toggles Rich Text permissions at runtime.
static void my_TMP_SetOnValidateInput(void* instance, void* value, void* methodInfo) {
    LOGI("SET onValidateInput field=%p delegate=%p", instance, value);
    if (orig_TMP_SetOnValidateInput)
        orig_TMP_SetOnValidateInput(instance, value, methodInfo);
    LogTMPInputState(instance, "set_onValidateInput sonrasi");
}

static void my_TMP_SetCharacterValidation(void* instance, int value, void* methodInfo) {
    LOGI("SET characterValidation field=%p value=%d", instance, value);
    if (orig_TMP_SetCharacterValidation)
        orig_TMP_SetCharacterValidation(instance, value, methodInfo);
    LogTMPInputState(instance, "set_characterValidation sonrasi");
}

static void my_TMP_SetInputValidator(void* instance, void* value, void* methodInfo) {
    LOGI("SET inputValidator field=%p validator=%p", instance, value);
    if (orig_TMP_SetInputValidator)
        orig_TMP_SetInputValidator(instance, value, methodInfo);
    LogTMPInputState(instance, "set_inputValidator sonrasi");
}

static void my_TMP_SetIsRichTextEditingAllowed(void* instance, int value, void* methodInfo) {
    LOGI("SET isRichTextEditingAllowed field=%p value=%d", instance, value);
    if (orig_TMP_SetIsRichTextEditingAllowed)
        orig_TMP_SetIsRichTextEditingAllowed(instance, value, methodInfo);
    LogTMPInputState(instance, "set_isRichTextEditingAllowed sonrasi");
}

static void my_TMP_SetRichText(void* instance, int value, void* methodInfo) {
    LOGI("SET richText field=%p value=%d", instance, value);
    if (orig_TMP_SetRichText)
        orig_TMP_SetRichText(instance, value, methodInfo);
    LogTMPInputState(instance, "set_richText sonrasi");
}

// ============================================================
// DX: CALL-CHAIN / TEXT-CHANGE DIAGNOSTICS
// ------------------------------------------------------------
// This layer is observation-only. It does not rewrite TMP text,
// Rich Text flags, validators, or name/chat values.
// ============================================================

#ifndef DX_USE_UNWIND
#define DX_USE_UNWIND 1
#endif

namespace dxdiag {

static const size_t kF_SoftKeyboard  = 0xB4;
static const size_t kF_TextComponent = 0xC8;
static const size_t kF_Text          = 0x160;

static const uintptr_t kRVA_TMP_SetText              = 0x4088800;
static const uintptr_t kRVA_TMP_UpdateLabel           = 0x4088998;
static const uintptr_t kRVA_TMP_SendOnValueChanged   = 0x4088F9C;
static const uintptr_t kRVA_TMP_SendOnVCAUL          = 0x408EB58;
static const uintptr_t kRVA_TMP_SendOnEndEdit        = 0x4094364;
static const uintptr_t kRVA_TMP_SendOnSubmit         = 0x4091D94;
static const uintptr_t kRVA_TMP_DeactivateInputField = 0x408C168;
static const uintptr_t kRVA_TSK_get_text             = kTSK_GetText_RVA;
static const uintptr_t kRVA_TSK_set_text             = 0x41B9770;
static const uintptr_t kRVA_TMPText_set_text         = 0x4097138;
static const uintptr_t kRVA_TMPText_SetText          = 0x409C76C;

static std::atomic<uint32_t> g_seq(0);
// Use 32-bit atomics on ARM32; 64-bit atomics may require libatomic.
static std::atomic<uint32_t> g_windowUntil(0);
static std::atomic<int> g_budget(0);
static std::atomic<void*> g_lastTextComp(nullptr);
static int64_t g_t0 = 0;
static const uint32_t kMaxEvents = 4000;

static int64_t nowMs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

static uint32_t nowMs32() {
    return static_cast<uint32_t>(nowMs());
}

static inline void* rdPtr(const void* base, size_t off) {
    if (!base) return nullptr;
    void* value = nullptr;
    memcpy(&value, reinterpret_cast<const uint8_t*>(base) + off, sizeof(value));
    return value;
}

static std::string str(const void* stringObject, int maxChars = 120) {
    if (!stringObject) return "<null>";
    const uint8_t* p = static_cast<const uint8_t*>(stringObject);
    int32_t len = -1;
    memcpy(&len, p + 8, sizeof(len));
    if (len < 0 || len > 1000000) return "<badlen>";
    const char16_t* c = reinterpret_cast<const char16_t*>(p + 12);
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "[%d]\"", len);
    std::string out = hdr;
    const int n = len < maxChars ? len : maxChars;
    for (int i = 0; i < n; ++i) {
        const char16_t ch = c[i];
        if (ch == '\n') out += "\\n";
        else if (ch == '\r') out += "\\r";
        else if (ch == '\t') out += "\\t";
        else if (ch < 0x20) {
            char b[8]; snprintf(b, sizeof(b), "\\x%02x", static_cast<unsigned>(ch)); out += b;
        } else if (ch < 0x80) {
            out += static_cast<char>(ch);
        } else if (ch < 0x800) {
            out += static_cast<char>(0xC0 | (ch >> 6));
            out += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (ch >> 12));
            out += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
    if (len > n) out += "...";
    out += "\"";
    return out;
}

static int lastChar(const void* stringObject) {
    if (!stringObject) return -1;
    const uint8_t* p = static_cast<const uint8_t*>(stringObject);
    int32_t len = -1;
    memcpy(&len, p + 8, sizeof(len));
    if (len <= 0 || len > 1000000) return -1;
    const char16_t* c = reinterpret_cast<const char16_t*>(p + 12);
    return static_cast<int>(c[len - 1]);
}

static bool hasSizeTag(const std::string& value) {
    std::string lower = value;
    for (size_t i = 0; i < lower.size(); ++i) {
        if (lower[i] >= 'A' && lower[i] <= 'Z') lower[i] = static_cast<char>(lower[i] + 32);
    }
    return lower.find("<size") != std::string::npos;
}

static bool isActiveField(const void* self) {
    return self && rdPtr(self, kF_SoftKeyboard) != nullptr;
}

static std::string describePc(uintptr_t pc) {
    char buf[160];
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(pc), &info) && info.dli_fname) {
        const char* slash = strrchr(info.dli_fname, '/');
        const char* name = slash ? slash + 1 : info.dli_fname;
        if (strstr(name, "libil2cpp.so")) {
            const uintptr_t base = g_il2cppLoadBias ? g_il2cppLoadBias
                                                    : reinterpret_cast<uintptr_t>(info.dli_fbase);
            snprintf(buf, sizeof(buf), "il2cpp+0x%" PRIxPTR, pc - base);
        } else {
            snprintf(buf, sizeof(buf), "%s+0x%" PRIxPTR, name,
                     pc - reinterpret_cast<uintptr_t>(info.dli_fbase));
        }
        return buf;
    }
    snprintf(buf, sizeof(buf), "?0x%" PRIxPTR, pc);
    return buf;
}

#if DX_USE_UNWIND
struct BacktraceData { uintptr_t pcs[16]; int count; };
static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg) {
    BacktraceData* data = static_cast<BacktraceData*>(arg);
    const uintptr_t pc = _Unwind_GetIP(context);
    if (!pc || data->count >= 16) return _URC_END_OF_STACK;
    data->pcs[data->count++] = pc;
    return _URC_NO_REASON;
}
#endif

static std::string chain(void* ret) {
    std::string out = "ret=" + describePc(reinterpret_cast<uintptr_t>(ret));
#if DX_USE_UNWIND
    BacktraceData bt{};
    _Unwind_Backtrace(unwindCallback, &bt);
    int shown = 0;
    for (int i = 0; i < bt.count && shown < 8; ++i) {
        const std::string item = describePc(bt.pcs[i]);
        if (item.find("CursedHouseChat") != std::string::npos ||
            item.find("multiplayermod") != std::string::npos) continue;
        out += (shown == 0 ? " | " : " <- ") + item;
        ++shown;
    }
#endif
    return out;
}

static void logEvent(const char* tag, const void* object, const std::string& info,
                     void* ret, bool withChain = true) {
    const uint32_t id = ++g_seq;
    if (id > kMaxEvents) {
        if (id == kMaxEvents + 1) LOGI("DX: olay siniri doldu, kayit durdu");
        return;
    }
    LOGI("DX#%u t=%lldms %s obj=%p %s", id,
         static_cast<long long>(nowMs() - g_t0), tag, object, info.c_str());
    if (withChain) LOGI("DX#%u zincir: %s", id, chain(ret).c_str());
}

static void openWindow(uint32_t ms, int budget) {
    g_budget.store(budget);
    g_windowUntil.store(nowMs32() + ms);
}

static bool windowActive() {
    const uint32_t until = g_windowUntil.load();
    if (!until) return false;
    // Signed subtraction remains valid across uint32_t timer wrap for short windows.
    if (static_cast<int32_t>(nowMs32() - until) > 0) {
        g_windowUntil.store(0);
        return false;
    }
    return g_budget.load() > 0;
}

static bool takeBudget() {
    int remaining = g_budget.load();
    while (remaining > 0) {
        if (g_budget.compare_exchange_weak(remaining, remaining - 1)) return true;
    }
    return false;
}

// Function signatures for ARM32 IL2CPP instance methods.
typedef void (*Fn_this_mi)(void*, void*);
typedef void (*Fn_this_str_mi)(void*, void*, void*);
typedef void (*Fn_this_str_bool_mi)(void*, void*, int, void*);
typedef void (*Fn_this_bool_mi)(void*, int, void*);
typedef void* (*Fn_this_retptr_mi)(void*, void*);

static Fn_this_str_bool_mi orig_SetText = nullptr;
static Fn_this_mi orig_UpdateLabel = nullptr;
static Fn_this_mi orig_SendOnValueChanged = nullptr;
static Fn_this_mi orig_SendOnVCAUL = nullptr;
static Fn_this_mi orig_SendOnEndEdit = nullptr;
static Fn_this_mi orig_SendOnSubmit = nullptr;
static Fn_this_bool_mi orig_Deactivate = nullptr;
static Fn_this_retptr_mi orig_TSK_get_text = nullptr;
static Fn_this_str_mi orig_TSK_set_text = nullptr;
static Fn_this_str_mi orig_TMPText_set_text = nullptr;
static Fn_this_str_mi orig_TMPText_SetText = nullptr;

static void my_SetText(void* self, void* value, int sendCallback, void* mi) {
    void* ret = __builtin_return_address(0);
    const std::string incoming = str(value, 180);
    const std::string before = self ? str(rdPtr(self, kF_Text), 180) : "<null-self>";
    const bool relevant = hasSizeTag(incoming) || hasSizeTag(before) ||
                          lastChar(value) == '>' || lastChar(rdPtr(self, kF_Text)) == '>';
    if (relevant) openWindow(1800, 180);
    if (relevant || windowActive()) {
        char cb[32]; snprintf(cb, sizeof(cb), " callback=%d", sendCallback);
        logEvent("TMP_InputField.SetText PRE", self,
                 "yeni=" + incoming + " eski=" + before + cb, ret, relevant);
    }
    if (orig_SetText) orig_SetText(self, value, sendCallback, mi);
    const std::string after = self ? str(rdPtr(self, kF_Text), 180) : "<null-self>";
    if (relevant || before != after) {
        logEvent("TMP_InputField.SetText POST", self,
                 "once=" + before + " sonra=" + after, ret, relevant);
    }
}

static void my_UpdateLabel(void* self, void* mi) {
    void* ret = __builtin_return_address(0);
    const std::string before = self ? str(rdPtr(self, kF_Text), 180) : "<null-self>";
    const bool relevant = hasSizeTag(before) || lastChar(rdPtr(self, kF_Text)) == '>';
    if (relevant) openWindow(1600, 120);
    if (relevant || windowActive())
        logEvent("TMP_InputField.UpdateLabel PRE", self, "m_Text=" + before, ret, relevant);
    if (orig_UpdateLabel) orig_UpdateLabel(self, mi);
    const std::string after = self ? str(rdPtr(self, kF_Text), 180) : "<null-self>";
    if (relevant || before != after)
        logEvent("TMP_InputField.UpdateLabel POST", self,
                 "once=" + before + " sonra=" + after, ret, relevant);
}

static void trackedValueChanged(const char* tag, void* self, void* mi,
                                Fn_this_mi original, void* ret) {
    g_lastTextComp.store(rdPtr(self, kF_TextComponent));
    void* beforePtr = rdPtr(self, kF_Text);
    const std::string before = str(beforePtr, 200);
    const bool tagged = hasSizeTag(before) || lastChar(beforePtr) == '>';
    if (tagged) openWindow(1800, 180);
    if (original) original(self, mi);
    const std::string after = str(rdPtr(self, kF_Text), 200);
    if (tagged || (windowActive() && before != after))
        logEvent(tag, self, "once=" + before + " sonra=" + after, ret, tagged);
}

static void my_SendOnValueChanged(void* self, void* mi) {
    trackedValueChanged("TMP_InputField.SendOnValueChanged", self, mi,
                         orig_SendOnValueChanged, __builtin_return_address(0));
}
static void my_SendOnVCAUL(void* self, void* mi) {
    trackedValueChanged("TMP_InputField.SendOnValueChangedAndUpdateLabel", self, mi,
                         orig_SendOnVCAUL, __builtin_return_address(0));
}

static void my_SendOnEndEdit(void* self, void* mi) {
    void* ret = __builtin_return_address(0);
    g_lastTextComp.store(rdPtr(self, kF_TextComponent));
    logEvent("TMP_InputField.SendOnEndEdit PRE", self,
             "metin=" + str(rdPtr(self, kF_Text), 200), ret);
    openWindow(2500, 180);
    if (orig_SendOnEndEdit) orig_SendOnEndEdit(self, mi);
    logEvent("TMP_InputField.SendOnEndEdit POST", self,
             "metin=" + str(rdPtr(self, kF_Text), 200), ret, false);
}

static void my_SendOnSubmit(void* self, void* mi) {
    void* ret = __builtin_return_address(0);
    g_lastTextComp.store(rdPtr(self, kF_TextComponent));
    logEvent("TMP_InputField.SendOnSubmit PRE", self,
             "metin=" + str(rdPtr(self, kF_Text), 200), ret);
    openWindow(2500, 180);
    if (orig_SendOnSubmit) orig_SendOnSubmit(self, mi);
    logEvent("TMP_InputField.SendOnSubmit POST", self,
             "metin=" + str(rdPtr(self, kF_Text), 200), ret, false);
}

static void my_Deactivate(void* self, int clearSelection, void* mi) {
    void* ret = __builtin_return_address(0);
    if (isActiveField(self) || windowActive()) {
        char cs[32]; snprintf(cs, sizeof(cs), " clearSelection=%d", clearSelection);
        logEvent("TMP_InputField.DeactivateInputField", self,
                 "metin=" + str(rdPtr(self, kF_Text), 160) + cs, ret);
    }
    if (orig_Deactivate) orig_Deactivate(self, clearSelection, mi);
}

// Observe the actual Android soft-keyboard text getter. It opens the short
// trace window when a partial/completed <size...> string enters this layer.
static void* my_TSK_get_text(void* self, void* mi) {
    void* ret = __builtin_return_address(0);
    void* value = orig_TSK_get_text ? orig_TSK_get_text(self, mi) : nullptr;
    const std::string valueText = str(value, 180);
    const bool relevant = hasSizeTag(valueText) || lastChar(value) == '>';
    if (relevant) openWindow(1800, 180);
    if (relevant || windowActive()) {
        static std::string lastLogged;
        if (valueText != lastLogged) {
            logEvent("TouchScreenKeyboard.get_text", self,
                     "deger=" + valueText, ret);
            lastLogged = valueText;
        }
    }
    return value;
}

static void my_TSK_set_text(void* self, void* value, void* mi) {
    void* ret = __builtin_return_address(0);
    const std::string valueText = str(value, 160);
    const bool relevant = hasSizeTag(valueText) || lastChar(value) == '>';
    if (relevant) openWindow(1800, 180);
    if (relevant || windowActive()) {
        static std::string lastLogged;
        if (valueText != lastLogged) {
            logEvent("TouchScreenKeyboard.set_text", self, "deger=" + valueText, ret);
            lastLogged = valueText;
        }
    }
    if (orig_TSK_set_text) orig_TSK_set_text(self, value, mi);
}

static void my_TMPText_set_text(void* self, void* value, void* mi) {
    if (windowActive() && takeBudget()) {
        void* ret = __builtin_return_address(0);
        const char* mark = (self == g_lastTextComp.load()) ? " [GIRIS-KUTUSU-METNI]" : "";
        logEvent("TMP_Text.set_text", self,
                 "yeni=" + str(value, 140) + mark, ret);
    }
    if (orig_TMPText_set_text) orig_TMPText_set_text(self, value, mi);
}

static void my_TMPText_SetText(void* self, void* value, void* mi) {
    if (windowActive() && takeBudget()) {
        void* ret = __builtin_return_address(0);
        const char* mark = (self == g_lastTextComp.load()) ? " [GIRIS-KUTUSU-METNI]" : "";
        logEvent("TMP_Text.SetText(string)", self,
                 "yeni=" + str(value, 140) + mark, ret);
    }
    if (orig_TMPText_SetText) orig_TMPText_SetText(self, value, mi);
}

static bool DxInstallDiagHooks() {
    g_t0 = nowMs();
    int ok = 0;
    int tried = 0;
#define DX_HOOK(rva, replacement, original, label) \
    do { \
        ++tried; \
        if (InstallArmHook((rva), reinterpret_cast<void*>(replacement), \
                           reinterpret_cast<void**>(&(original)), (label))) ++ok; \
    } while (0)

    DX_HOOK(kRVA_TMP_SetText, my_SetText, orig_SetText,
            "DX TMP_InputField::SetText(string,bool)");
    DX_HOOK(kRVA_TMP_UpdateLabel, my_UpdateLabel, orig_UpdateLabel,
            "DX TMP_InputField::UpdateLabel");
    DX_HOOK(kRVA_TMP_SendOnValueChanged, my_SendOnValueChanged, orig_SendOnValueChanged,
            "DX TMP_InputField::SendOnValueChanged");
    DX_HOOK(kRVA_TMP_SendOnVCAUL, my_SendOnVCAUL, orig_SendOnVCAUL,
            "DX TMP_InputField::SendOnValueChangedAndUpdateLabel");
    DX_HOOK(kRVA_TMP_SendOnEndEdit, my_SendOnEndEdit, orig_SendOnEndEdit,
            "DX TMP_InputField::SendOnEndEdit");
    DX_HOOK(kRVA_TMP_SendOnSubmit, my_SendOnSubmit, orig_SendOnSubmit,
            "DX TMP_InputField::SendOnSubmit");
    DX_HOOK(kRVA_TMP_DeactivateInputField, my_Deactivate, orig_Deactivate,
            "DX TMP_InputField::DeactivateInputField");
    DX_HOOK(kRVA_TSK_get_text, my_TSK_get_text, orig_TSK_get_text,
            "DX TouchScreenKeyboard::get_text");
    DX_HOOK(kRVA_TSK_set_text, my_TSK_set_text, orig_TSK_set_text,
            "DX TouchScreenKeyboard::set_text");
    DX_HOOK(kRVA_TMPText_set_text, my_TMPText_set_text, orig_TMPText_set_text,
            "DX TMP_Text::set_text");
    DX_HOOK(kRVA_TMPText_SetText, my_TMPText_SetText, orig_TMPText_SetText,
            "DX TMP_Text::SetText(string)");
#undef DX_HOOK
    LOGI("DX: ek teshis kancalari %d/%d basarili", ok, tried);
    return ok > 0;
}

} // namespace dxdiag

// ============================================================
// HOOK INSTALLATION
// ============================================================

static bool InstallHooks() {
    if (!g_il2cppLoadBias) {
        LOGE("libil2cpp load bias henuz hazir degil");
        return false;
    }

    LOGI("--- 32-BIT INPUT DIAGNOSTIC KANCALARI KURULUYOR ---");
    LOGI("TMP_InputField offsets: limit=0x114 validation=0x108 onValidate=0x138 validator=0x1F0 richText=0x16D richEdit=0x1E6 keyboard=0xB4 text=0x160");

    int successCount = 0;
    int attemptedCount = 0;

#define TRY_HOOK(rva, replacement, original, label) \
    do { \
        ++attemptedCount; \
        if (InstallArmHook((rva), reinterpret_cast<void*>(replacement), \
                reinterpret_cast<void**>(&(original)), (label))) ++successCount; \
    } while (0)

    // Existing limit hooks (kept)
    TRY_HOOK(kTMP_ActivateInputFieldInternal_RVA,
             my_TMP_ActivateInputFieldInternal,
             orig_TMP_ActivateInputFieldInternal,
             "TMP_InputField::ActivateInputFieldInternal");
    TRY_HOOK(kTMP_AppendChar_RVA, my_TMP_AppendChar,
             orig_TMP_AppendChar, "TMP_InputField::Append(char)");
    TRY_HOOK(kTMP_InsertChar_RVA, my_TMP_InsertChar,
             orig_TMP_InsertChar, "TMP_InputField::Insert(char)");
    TRY_HOOK(kTMP_AppendString_RVA, my_TMP_AppendString,
             orig_TMP_AppendString, "TMP_InputField::Append(string)");
    TRY_HOOK(kTMP_SetCharacterLimit_RVA, my_TMP_SetCharacterLimit,
             orig_TMP_SetCharacterLimit, "TMP_InputField::set_characterLimit");
    TRY_HOOK(kTSK_SetCharacterLimit_RVA, my_TSK_SetCharacterLimit,
             orig_TSK_SetCharacterLimit, "TouchScreenKeyboard::set_characterLimit");

    // New diagnostic hooks
    TRY_HOOK(kTMP_Validate_RVA, my_TMP_Validate,
             orig_TMP_Validate, "TMP_InputField::Validate");
    TRY_HOOK(kTMP_IsValidChar_RVA, my_TMP_IsValidChar,
             orig_TMP_IsValidChar, "TMP_InputField::IsValidChar");
    TRY_HOOK(kTMP_UpdateStringPositionFromKeyboard_RVA,
             my_TMP_UpdateStringPositionFromKeyboard,
             orig_TMP_UpdateStringPositionFromKeyboard,
             "TMP_InputField::UpdateStringPositionFromKeyboard");
    TRY_HOOK(kTMP_SetOnValidateInput_RVA, my_TMP_SetOnValidateInput,
             orig_TMP_SetOnValidateInput, "TMP_InputField::set_onValidateInput");
    TRY_HOOK(kTMP_SetCharacterValidation_RVA, my_TMP_SetCharacterValidation,
             orig_TMP_SetCharacterValidation, "TMP_InputField::set_characterValidation");
    TRY_HOOK(kTMP_SetInputValidator_RVA, my_TMP_SetInputValidator,
             orig_TMP_SetInputValidator, "TMP_InputField::set_inputValidator");
    TRY_HOOK(kTMP_SetIsRichTextEditingAllowed_RVA,
             my_TMP_SetIsRichTextEditingAllowed,
             orig_TMP_SetIsRichTextEditingAllowed,
             "TMP_InputField::set_isRichTextEditingAllowed");
    TRY_HOOK(kTMP_SetRichText_RVA, my_TMP_SetRichText,
             orig_TMP_SetRichText, "TMP_InputField::set_richText");

#undef TRY_HOOK

    // Additional call-chain tracing for <size> completion and submit-time checks.
    dxdiag::DxInstallDiagHooks();

    LOGI("========== KANCA KURULUMU BITTI: %d/%d base kanca basarili ==========",
         successCount, attemptedCount);
    LOGI("Test: <size=999> yazip > karakterinden sonra 2-3 sn bekle; sonra geri silme/yeni karakter dene. Enter/Done ile isim kaydini da ayri test et.");
    LOGI("Logcat filtre: CursedHouseChat");

    return successCount > 0;
}

// ============================================================
// THREAD / JNI
// ============================================================

static void* hack_thread(void*) {
    LOGI("CursedHouseChat diagnostic: hack_thread basladi; libil2cpp.so bekleniyor...");

    while (g_il2cppLoadBias == 0) {
        g_il2cppLoadBias = GetModuleLoadBias("libil2cpp.so");
        if (g_il2cppLoadBias == 0) sleep(1);
    }

    LOGI("libil2cpp load bias=0x%" PRIxPTR, g_il2cppLoadBias);
    InstallHooks();
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm;
    (void)reserved;

    pthread_t ptid;
    const int rc = pthread_create(&ptid, nullptr, hack_thread, nullptr);
    if (rc != 0) {
        LOGE("hack_thread olusturulamadi: %d", rc);
    } else {
        pthread_detach(ptid);
    }

    return JNI_VERSION_1_6;
}
