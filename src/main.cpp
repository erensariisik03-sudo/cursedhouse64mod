#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <vector>
#include <string>
#include "arm_hook.h"

#define LOG_TAG "CursedHouseChat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================
// 32-BIT RVA TANIMLARI - yeni dump.cs'den
// ============================================================

// ChatManager.inputField alanı dump.cs'de 0x18 olarak görünüyor.
// Bu dosyada ChatManager::Update hook'u kurulmadığı için bu sabit kullanılmıyor.
static constexpr uintptr_t kChat_InputField_FieldOffset = 0x18;

// TMP_InputField::m_CharacterLimit alanı yeni dump.cs'de de 0x114.
static constexpr uintptr_t kTMP_CharacterLimit_FieldOffset = 0x114;

// TMPro.TMP_InputField
static constexpr uintptr_t kTMP_SetCharacterLimit_RVA = 0x4089A20;
static constexpr uintptr_t kTMP_AppendString_RVA = 0x4093D2C;
static constexpr uintptr_t kTMP_AppendChar_RVA = 0x4093DD4;
static constexpr uintptr_t kTMP_InsertChar_RVA = 0x4094248; // Insert(char c)
static constexpr uintptr_t kTMP_ActivateInputFieldInternal_RVA = 0x408DCE4;

// UnityEngine.TouchScreenKeyboard::set_characterLimit
static constexpr uintptr_t kTSK_SetCharacterLimit_RVA = 0x41B9C4C;

// UnityEngine.UIElements.TextElement getters.
// Adresler dump.cs'de bulundu; oyunun bu UIElements yolunu kullanıp kullanmadığı kesin değil.
static constexpr uintptr_t kUIE_GetMaxLength_RVA = 0x437EB78; // TextElement::get_maxLength()
static constexpr uintptr_t kUIE_ITextEdition_GetMaxLength_RVA = 0x437EA34; // TextElement explicit interface getter

// ============================================================
// FUNCTION POINTER SIGNATURES
// ============================================================
typedef void (*VoidInstanceFn)(void* instance, void* methodInfo);
typedef void (*IntInstanceFn)(void* instance, int value, void* methodInfo);
typedef void (*StringInstanceFn)(void* instance, void* stringObject, void* methodInfo);
typedef void (*CharInstanceFn)(void* instance, uint32_t ch, void* methodInfo);
typedef int (*GetIntInstanceFn)(void* instance, void* methodInfo);

static VoidInstanceFn orig_TMP_ActivateInputFieldInternal = nullptr;
static IntInstanceFn orig_TMP_SetCharacterLimit = nullptr;
static IntInstanceFn orig_TSK_SetCharacterLimit = nullptr;
static StringInstanceFn orig_TMP_AppendString = nullptr;
static CharInstanceFn orig_TMP_AppendChar = nullptr;
static CharInstanceFn orig_TMP_InsertChar = nullptr;
static GetIntInstanceFn orig_UIE_GetMaxLength = nullptr;
static GetIntInstanceFn orig_UIE_ITextEdition_GetMaxLength = nullptr;

static uintptr_t g_il2cppLoadBias = 0;

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

static bool InstallArmHook(uintptr_t rva, void* replacement, void** original, const char* label) {
    const uintptr_t target = RvaToFunctionAddress(rva);
    if (target == 0) {
        LOGE("Adres 0: %s RVA=0x%" PRIxPTR, label, rva);
        return false;
    }

    const int rc = ArmHook(reinterpret_cast<void*>(target), replacement, original);
    if (rc == 0 && original && *original) {
        LOGI("Kanca BASARILI: %s (RVA: 0x%" PRIxPTR ", target: %p)",
             label, rva, reinterpret_cast<void*>(target));
        return true;
    }

    LOGE("Kanca BASARISIZ: %s (RVA: 0x%" PRIxPTR ", hata: %d, original: %p)",
         label, rva, rc, original ? *original : nullptr);
    return false;
}

// ============================================================
// TMP INPUT FIELD CHARACTER LIMIT
// ============================================================
static inline int ReadInt(uintptr_t address) {
    return *reinterpret_cast<int*>(address);
}

static inline void WriteInt(uintptr_t address, int value) {
    *reinterpret_cast<int*>(address) = value;
}

static void ForceTMPObjectLimit(void* tmpInstance, const char* reason) {
    if (!tmpInstance) return;

    const uintptr_t base = reinterpret_cast<uintptr_t>(tmpInstance);
    const uintptr_t addr = base + kTMP_CharacterLimit_FieldOffset;
    const int oldValue = ReadInt(addr);

    // TMP_InputField characterLimit: 0 is the unlimited setting.
    // Negative values are also left alone as they may already mean no limit.
    if (oldValue != 0 && oldValue != -1) {
        WriteInt(addr, 0);
        LOGI("TMP LIMIT SIFIRLANDI: kutu=%p sebep=%s eski=%d yeni=0",
             tmpInstance, reason, oldValue);
    }
}

// ============================================================
// UIELEMENTS GETTER HOOKS (UNCERTAIN PATH)
// ============================================================
// Unity UIElements uses -1 to represent no character limit.
// These hooks are retained as fallback hooks; the chat field in dump.cs is TMP_InputField.
static int my_UIE_GetMaxLength(void* instance, void* methodInfo) {
    (void)instance;
    (void)methodInfo;
    LOGI("TETIKLENDI: TextElement::get_maxLength -> -1 (sinirsiz)");
    return -1;
}

static int my_UIE_ITextEdition_GetMaxLength(void* instance, void* methodInfo) {
    (void)instance;
    (void)methodInfo;
    LOGI("TETIKLENDI: TextElement::ITextEdition.get_maxLength -> -1 (sinirsiz)");
    return -1;
}

// ============================================================
// TMP HOOKS
// ============================================================
static void my_TMP_ActivateInputFieldInternal(void* instance, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::ActivateInputFieldInternal");
    ForceTMPObjectLimit(instance, "ActivateInputFieldInternal");

    if (orig_TMP_ActivateInputFieldInternal)
        orig_TMP_ActivateInputFieldInternal(instance, methodInfo);

    // Orijinal metot alanı yeniden değiştirdiyse sonrasında tekrar kontrol et.
    ForceTMPObjectLimit(instance, "ActivateInputFieldInternal sonrasi");
}

static void my_TMP_AppendChar(void* instance, uint32_t ch, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::Append(char), kod=%u", ch);
    ForceTMPObjectLimit(instance, "Append(char) oncesi");

    if (orig_TMP_AppendChar)
        orig_TMP_AppendChar(instance, ch, methodInfo);

    ForceTMPObjectLimit(instance, "Append(char) sonrasi");
}

static void my_TMP_InsertChar(void* instance, uint32_t ch, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::Insert(char), kod=%u", ch);
    ForceTMPObjectLimit(instance, "Insert(char) oncesi");

    if (orig_TMP_InsertChar)
        orig_TMP_InsertChar(instance, ch, methodInfo);

    ForceTMPObjectLimit(instance, "Insert(char) sonrasi");
}

static void my_TMP_AppendString(void* instance, void* stringObject, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::Append(string)");
    ForceTMPObjectLimit(instance, "Append(string) oncesi");

    if (orig_TMP_AppendString)
        orig_TMP_AppendString(instance, stringObject, methodInfo);

    ForceTMPObjectLimit(instance, "Append(string) sonrasi");
}

static void my_TMP_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InputField::set_characterLimit istenen=%d -> 0", value);

    // Orijinal setter'ı 0 ile çağırmak, setter'ın kendi yan etkilerini de çalıştırır.
    if (orig_TMP_SetCharacterLimit)
        orig_TMP_SetCharacterLimit(instance, 0, methodInfo);

    ForceTMPObjectLimit(instance, "set_characterLimit");
}

static void my_TSK_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("TETIKLENDI: TouchScreenKeyboard::set_characterLimit istenen=%d -> 0", value);
    if (orig_TSK_SetCharacterLimit)
        orig_TSK_SetCharacterLimit(instance, 0, methodInfo);
}

// ============================================================
// HOOK INSTALLATION
// ============================================================
static bool InstallHooks() {
    if (!g_il2cppLoadBias) {
        LOGE("libil2cpp load bias henuz hazir degil");
        return false;
    }

    LOGI("--- KANCALAR KURULUYOR ---");
    LOGI("TMP_InputField.m_CharacterLimit offset=0x%" PRIxPTR,
         kTMP_CharacterLimit_FieldOffset);
    LOGI("ChatManager.inputField offset=0x%" PRIxPTR " (bu kaynakta dogrudan kullanilmiyor)",
         kChat_InputField_FieldOffset);

    int successCount = 0;
    int attemptedCount = 0;

#define TRY_HOOK(rva, replacement, original, label) \
    do { ++attemptedCount; if (InstallArmHook((rva), reinterpret_cast<void*>(replacement), \
             reinterpret_cast<void**>(&(original)), (label))) ++successCount; } while (0)

    // UIElements yedek kancalari. Oyunun bu metin girisi yolunu kullandigi kesin degil.
    TRY_HOOK(kUIE_GetMaxLength_RVA, my_UIE_GetMaxLength,
             orig_UIE_GetMaxLength, "TextElement::get_maxLength [yedek/riskli]");
    TRY_HOOK(kUIE_ITextEdition_GetMaxLength_RVA, my_UIE_ITextEdition_GetMaxLength,
             orig_UIE_ITextEdition_GetMaxLength, "TextElement::ITextEdition.get_maxLength [yedek/riskli]");

    // TMP_InputField ana kancalari - yeni dump.cs ile eslestirildi.
    TRY_HOOK(kTMP_ActivateInputFieldInternal_RVA, my_TMP_ActivateInputFieldInternal,
             orig_TMP_ActivateInputFieldInternal, "TMP_InputField::ActivateInputFieldInternal");
    TRY_HOOK(kTMP_AppendChar_RVA, my_TMP_AppendChar,
             orig_TMP_AppendChar, "TMP_InputField::Append(char)");
    TRY_HOOK(kTMP_InsertChar_RVA, my_TMP_InsertChar,
             orig_TMP_InsertChar, "TMP_InputField::Insert(char)");
    TRY_HOOK(kTMP_AppendString_RVA, my_TMP_AppendString,
             orig_TMP_AppendString, "TMP_InputField::Append(string)");
    TRY_HOOK(kTMP_SetCharacterLimit_RVA, my_TMP_SetCharacterLimit,
             orig_TMP_SetCharacterLimit, "TMP_InputField::set_characterLimit");

    // UnityEngine.TouchScreenKeyboard fallback setter.
    TRY_HOOK(kTSK_SetCharacterLimit_RVA, my_TSK_SetCharacterLimit,
             orig_TSK_SetCharacterLimit, "TouchScreenKeyboard::set_characterLimit");

#undef TRY_HOOK

    LOGI("========== KANCA KURULUMU BITTI: %d/%d basarili ==========\n", successCount, attemptedCount);
    LOGI("Oyunda sohbet alanina dokunup yazi yazin; logcat CursedHouseChat etiketini kontrol edin.");

    return successCount > 0;
}

// ============================================================
// THREAD / JNI
// ============================================================
static void* hack_thread(void*) {
    LOGI("CursedHouseChat: hack_thread basladi; libil2cpp.so bekleniyor...");

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
