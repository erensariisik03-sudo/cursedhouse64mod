#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
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

    LOGI("========== KANCA KURULUMU BITTI: %d/%d basarili ==========",
         successCount, attemptedCount);
    LOGI("Test: oyuncu adi, oda adi ve sohbet alanlarinda <size=999> girisini ayri ayri dene.");
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

    LOGI("========== KANCA KURULUMU BITTI: %d/%d basarili ==========",
         successCount, attemptedCount);
    LOGI("Test: oyuncu adi, oda adi ve sohbet alanlarinda <size=999> girisini ayri ayri dene.");
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
