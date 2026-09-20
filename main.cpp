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

// -----------------------------
// RVA TANIMLAMALARI
// -----------------------------
static constexpr uintptr_t kChat_Update_RVA = 0xF6A6CC;
static constexpr uintptr_t kChat_InputField_FieldOffset = 0x18;
static constexpr uintptr_t kTMP_CharacterLimit_FieldOffset = 0x114;

// TMP / TouchScreenKeyboard (Yedek amaçlı tetikleyici hooklar)
static constexpr uintptr_t kTMP_SetCharacterLimit_RVA = 0x34AA4D0;
static constexpr uintptr_t kTMP_AppendString_RVA = 0x34B47DC;
static constexpr uintptr_t kTMP_AppendChar_RVA = 0x34B4884;
static constexpr uintptr_t kTMP_InsertChar_RVA = 0x34B4CF8;
static constexpr uintptr_t kTMP_ActivateInputFieldInternal_RVA = 0x34AE794;
static constexpr uintptr_t kTSK_SetCharacterLimit_RVA = 0x3598790;

// YENİ: UIElements Getter Fonksiyonları (Oyunun limiti okuduğu anlar)
static constexpr uintptr_t kUIE_GetMaxLength_RVA = 0x37B3FD0;
static constexpr uintptr_t kUIE_ITextEdition_GetMaxLength_RVA = 0x37B3E8C;

// -----------------------------
// FONKSİYON İMZALARI (TYPEDEFS)
// -----------------------------
typedef void (*VoidInstanceFn)(void* instance, void* methodInfo);
typedef void (*IntInstanceFn)(void* instance, int value, void* methodInfo);
typedef void (*StringInstanceFn)(void* instance, void* stringObject, void* methodInfo);
typedef void (*CharInstanceFn)(void* instance, uint32_t ch, void* methodInfo);
typedef int (*GetIntInstanceFn)(void* instance, void* methodInfo);

// Orijinal fonksiyon işaretçileri
static VoidInstanceFn orig_chat_Update = nullptr;
static IntInstanceFn orig_TMP_SetCharacterLimit = nullptr;
static IntInstanceFn orig_TSK_SetCharacterLimit = nullptr;
static VoidInstanceFn orig_TMP_ActivateInputFieldInternal = nullptr;
static StringInstanceFn orig_TMP_AppendString = nullptr;
static CharInstanceFn orig_TMP_AppendChar = nullptr;
static CharInstanceFn orig_TMP_InsertChar = nullptr;
static GetIntInstanceFn orig_UIE_GetMaxLength = nullptr;
static GetIntInstanceFn orig_UIE_ITextEdition_GetMaxLength = nullptr;

static uintptr_t g_il2cppLoadBias = 0;

// -----------------------------
// BELLEK (MEMORY) YARDIMCILARI
// -----------------------------
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
            &e.start, &e.end, e.perms, &e.offset, &devMajor, &devMinor, &inode, pathname);
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

static inline uintptr_t MakeFunctionAddress(uintptr_t address) {
    return address;
}

static inline uintptr_t RvaToFunctionAddress(uintptr_t rva) {
    return MakeFunctionAddress(g_il2cppLoadBias + rva);
}

static bool InstallArmHook(uintptr_t rva, void* replacement, void** original, const char* label) {
    const uintptr_t target = RvaToFunctionAddress(rva);
    if (target == 0) return false;
    const int rc = ArmHook(reinterpret_cast<void*>(target), replacement, original);
    if (rc == 0 && original && *original) {
        LOGI("Kanca Basarili: %s (RVA: 0x%" PRIxPTR ")", label, rva);
        return true;
    } else {
        LOGE("Kanca BASARISIZ: %s (Hata Kodu: %d)", label, rc);
        return false;
    }
}

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
    if (oldValue != 0 && oldValue != -1) { 
        WriteInt(addr, 0);
        LOGI("TMP LIMIT SIFIRLANDI: Kutu=%p Sebep=%s EskiLimit=%d YeniLimit=0", tmpInstance, reason, oldValue);
    }
}

// -----------------------------
// KANCA (HOOK) FONKSİYONLARI
// -----------------------------

// --- YENİ EKLENEN OKUMA (GET) KANCALARI ---
static int my_UIE_GetMaxLength(void* instance, void* methodInfo) {
    LOGI("TETIKLENDI: UIE_GetMaxLength çağrıldı. Orijinal limiti okumadan 0 (Sınırsız) döndürülüyor.");
    // Not: UIElements bazen -1 bekleyebilir. 0 çalışmazsa burayı logcat sonrasında -1 yapacağız.
    return 0; 
}

static int my_UIE_ITextEdition_GetMaxLength(void* instance, void* methodInfo) {
    LOGI("TETIKLENDI: UIE_ITextEdition_GetMaxLength çağrıldı. Orijinal limiti okumadan 0 döndürülüyor.");
    return 0; 
}

// --- ESKİ YAZMA / ETKİLEŞİM KANCALARI (Loglamak için tutuyoruz) ---
static void my_TMP_ActivateInputFieldInternal(void* instance, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_ActivateInputFieldInternal (Metin Kutusuna Tıklandı)");
    ForceTMPObjectLimit(instance, "Activate (Tıklama Anı)");
    if (orig_TMP_ActivateInputFieldInternal) orig_TMP_ActivateInputFieldInternal(instance, methodInfo);
}

static void my_TMP_AppendChar(void* instance, uint32_t ch, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_AppendChar (Girilen Harf Kodu: %u)", ch);
    ForceTMPObjectLimit(instance, "AppendChar (Harf Girme)");
    if (orig_TMP_AppendChar) orig_TMP_AppendChar(instance, ch, methodInfo);
}

static void my_TMP_InsertChar(void* instance, uint32_t ch, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_InsertChar (Girilen Harf Kodu: %u)", ch);
    ForceTMPObjectLimit(instance, "InsertChar (Araya Harf Girme)");
    if (orig_TMP_InsertChar) orig_TMP_InsertChar(instance, ch, methodInfo);
}

static void my_TMP_AppendString(void* instance, void* stringObject, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_AppendString (Metin Yapıştırma/Ekleme)");
    ForceTMPObjectLimit(instance, "AppendString (Metin Yapıştırma)");
    if (orig_TMP_AppendString) orig_TMP_AppendString(instance, stringObject, methodInfo);
}

static void my_TMP_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("TETIKLENDI: TMP_SetCharacterLimit (İstenen Limit: %d) -> Engellendi.", value);
    ForceTMPObjectLimit(instance, "Setter Hook (Zorlama)");
}

static void my_TSK_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("TETIKLENDI: TouchScreenKeyboard.set_characterLimit (İstenen Limit: %d) -> 0 olarak değiştiriliyor.", value);
    if (orig_TSK_SetCharacterLimit) orig_TSK_SetCharacterLimit(instance, 0, methodInfo);
}

// -----------------------------
// ANA KURULUM THREAD'İ
// -----------------------------
static void* hack_thread(void*) {
    LOGI("CursedHouseChat: Hack thread basladi, libil2cpp.so bekleniyor...");
    
    while (g_il2cppLoadBias == 0) {
        g_il2cppLoadBias = GetModuleLoadBias("libil2cpp.so");
        if (g_il2cppLoadBias == 0) sleep(1);
    }
    
    LOGI("libil2cpp load bias=0x%" PRIxPTR, g_il2cppLoadBias);
    LOGI("--- KANCALAR KURULUYOR ---");

    // YENİ: Okuma (Getter) Kancaları 
    InstallArmHook(kUIE_GetMaxLength_RVA, reinterpret_cast<void*>(my_UIE_GetMaxLength), reinterpret_cast<void**>(&orig_UIE_GetMaxLength), "UIE_GetMaxLength");
    InstallArmHook(kUIE_ITextEdition_GetMaxLength_RVA, reinterpret_cast<void*>(my_UIE_ITextEdition_GetMaxLength), reinterpret_cast<void**>(&orig_UIE_ITextEdition_GetMaxLength), "UIE_ITextEdition_GetMaxLength");

    // Aktif Kullanım Kancaları
    InstallArmHook(kTMP_ActivateInputFieldInternal_RVA, reinterpret_cast<void*>(my_TMP_ActivateInputFieldInternal), reinterpret_cast<void**>(&orig_TMP_ActivateInputFieldInternal), "TMP_ActivateInputFieldInternal");
    InstallArmHook(kTMP_AppendChar_RVA, reinterpret_cast<void*>(my_TMP_AppendChar), reinterpret_cast<void**>(&orig_TMP_AppendChar), "TMP_AppendChar");
    InstallArmHook(kTMP_InsertChar_RVA, reinterpret_cast<void*>(my_TMP_InsertChar), reinterpret_cast<void**>(&orig_TMP_InsertChar), "TMP_InsertChar");
    InstallArmHook(kTMP_AppendString_RVA, reinterpret_cast<void*>(my_TMP_AppendString), reinterpret_cast<void**>(&orig_TMP_AppendString), "TMP_AppendString");

    // Güvenlik Kancaları
    InstallArmHook(kTMP_SetCharacterLimit_RVA, reinterpret_cast<void*>(my_TMP_SetCharacterLimit), reinterpret_cast<void**>(&orig_TMP_SetCharacterLimit), "TMP_SetCharacterLimit");
    InstallArmHook(kTSK_SetCharacterLimit_RVA, reinterpret_cast<void*>(my_TSK_SetCharacterLimit), reinterpret_cast<void**>(&orig_TSK_SetCharacterLimit), "TSK_SetCharacterLimit");

    LOGI("========== TUM KANCALAR KURULDU ==========");
    LOGI("Lutfen oyunda metin kutusuna tiklayin, klavyeyi acin, yazi yazin ve adb logcat ciktilarini paylasin.");
    
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm;
    (void)reserved;
    pthread_t thread;
    const int rc = pthread_create(&thread, nullptr, hack_thread, nullptr);
    if (rc != 0) {
        LOGE("hack_thread olusturulamadi: %d", rc);
    } else {
        pthread_detach(thread);
    }
    return JNI_VERSION_1_6;
}
