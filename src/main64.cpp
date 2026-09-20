#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <vector>
#include <atomic>

#include "arm64_hook.h"

#define LOG_TAG "CursedHouseChat64"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================================
// 64-BIT / ARM64 ADDRESSES VERIFIED AGAINST THE SUPPLIED dump.cs + veri.txt
// ============================================================================
// ARM32 used loadBias + RVA + Thumb(+1).
// ARM64 uses loadBias + RVA. There is NO +1 and NO -0x10000 adjustment.

// Game-specific chat class:
//   public class chat : MonoBehaviourPunCallbacks
//   private TMP_InputField inputField; // 0x30
static constexpr uintptr_t kChat_Update_RVA    = 0x1F22B2C;
static constexpr uintptr_t kChat_InputField_Offset = 0x30;

// TMP_InputField
static constexpr uintptr_t kTMP_GetCharacterLimit_RVA = 0x3F3A6BC;
static constexpr uintptr_t kTMP_SetCharacterLimit_RVA = 0x3F3A6C4;
static constexpr uintptr_t kTMP_AppendString_RVA = 0x3F43124;
static constexpr uintptr_t kTMP_AppendChar_RVA = 0x3F431D4;
static constexpr uintptr_t kTMP_InsertChar_RVA = 0x3F43548;
static constexpr uintptr_t kTMP_UpdateTouchKeyboard_RVA = 0x3F40C18;
static constexpr uintptr_t kTMP_ActivateInputFieldInternal_RVA = 0x3F3E0AC;
static constexpr uintptr_t kTMP_CharacterLimit_Offset = 0x19C;

// UIElements maxLength routes seen in the 64-bit dump.
static constexpr uintptr_t kUIE_ITextEdition_GetMaxLength_RVA = 0x41A75C8;
static constexpr uintptr_t kUIE_ITextEdition_SetMaxLength_RVA = 0x41A75D0;
static constexpr uintptr_t kUIE_GetMaxLength_RVA = 0x41A76F8;

// TouchScreenKeyboard
static constexpr uintptr_t kTSK_SetCharacterLimit_RVA = 0x3FEFCC0;
static constexpr uintptr_t kTSK_SetCharacterLimit_Injected_RVA = 0x3FEFD14;

// ============================================================================
// ABI / function types
// ============================================================================
using MethodInfoPtr = void*;

using VoidInstanceFn = void (*)(void*, MethodInfoPtr);
using IntGetterFn = int (*)(void*, MethodInfoPtr);
using IntSetterFn = void (*)(void*, int, MethodInfoPtr);
using CharInstanceFn = void (*)(void*, uint32_t, MethodInfoPtr);

// TouchScreenKeyboard_Injected setter/helper are static methods. The explicit
// native object/argument pointers arrive in x0/x1/x2 followed by MethodInfo*.
using TSKInjectedSetterFn = void (*)(void*, int, MethodInfoPtr);

// ============================================================================
// Originals
// ============================================================================
static VoidInstanceFn orig_chat_Update = nullptr;

static IntGetterFn orig_TMP_GetCharacterLimit = nullptr;
static IntSetterFn orig_TMP_SetCharacterLimit = nullptr;
static CharInstanceFn orig_TMP_AppendChar = nullptr;
static CharInstanceFn orig_TMP_InsertChar = nullptr;
static VoidInstanceFn orig_TMP_UpdateTouchKeyboard = nullptr;
static VoidInstanceFn orig_TMP_ActivateInputFieldInternal = nullptr;

static IntGetterFn orig_UIE_ITextEdition_GetMaxLength = nullptr;
static IntSetterFn orig_UIE_ITextEdition_SetMaxLength = nullptr;
static IntGetterFn orig_UIE_GetMaxLength = nullptr;

static IntSetterFn orig_TSK_SetCharacterLimit = nullptr;
static TSKInjectedSetterFn orig_TSK_SetCharacterLimit_Injected = nullptr;

static uintptr_t g_il2cppLoadBias = 0;
static std::atomic<void*> g_lastChat{nullptr};
static std::atomic<void*> g_lastInputField{nullptr};

// ============================================================================
// /proc/self/maps
// ============================================================================
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
        unsigned int devMajor = 0;
        unsigned int devMinor = 0;
        unsigned long inode = 0;
        char pathname[512] = {0};

        const int n = sscanf(
            line,
            "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %x:%x %lu %511[^\n]",
            &e.start,
            &e.end,
            e.perms,
            &e.offset,
            &devMajor,
            &devMinor,
            &inode,
            pathname
        );

        if (n >= 7) {
            if (n >= 8) {
                strncpy(e.path, pathname, sizeof(e.path) - 1);
                char* p = e.path;
                while (*p == ' ') ++p;
                if (p != e.path) {
                    memmove(e.path, p, strlen(p) + 1);
                }
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
    // IMPORTANT: AArch64 has no Thumb bit.
    return g_il2cppLoadBias + rva;
}

static bool Install64Hook(uintptr_t rva,
                          void* replacement,
                          void** original,
                          const char* label) {
    const uintptr_t target = RvaToFunctionAddress(rva);
    const int rc = Arm64Hook(
        reinterpret_cast<void*>(target),
        replacement,
        original
    );

    if (rc == 0) {
        LOGI("Hook OK: %s RVA=0x%" PRIxPTR " target=%p original=%p",
             label,
             rva,
             reinterpret_cast<void*>(target),
             original ? *original : nullptr);
        return true;
    }

    LOGE("Hook FAIL: %s RVA=0x%" PRIxPTR " rc=%d",
         label, rva, rc);
    return false;
}

// ============================================================================
// Field helpers
// ============================================================================
static inline void ForceUnlimitedInputField(void* inputField, const char* reason) {
    if (!inputField) return;

    const uintptr_t address =
        reinterpret_cast<uintptr_t>(inputField) + kTMP_CharacterLimit_Offset;

    int* limit = reinterpret_cast<int*>(address);
    const int oldValue = *limit;

    if (oldValue != 0) {
        *limit = 0;
        LOGI("64-bit TMP limit: field=%p old=%d -> 0 (%s)",
             inputField,
             oldValue,
             reason);
    }

    void* previous = g_lastInputField.exchange(inputField);
    if (previous != inputField) {
        LOGI("64-bit chat inputField=%p characterLimit=%d",
             inputField,
             *limit);
    }
}

static inline void* GetChatInputField(void* chatInstance) {
    if (!chatInstance) return nullptr;

    return *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(chatInstance) + kChat_InputField_Offset
    );
}

static inline void ForceChatLimit(void* chatInstance, const char* reason) {
    if (!chatInstance) return;

    g_lastChat.store(chatInstance);
    ForceUnlimitedInputField(GetChatInputField(chatInstance), reason);
}

// ============================================================================
// Chat hook
// ============================================================================
static void my_chat_Update(void* instance, MethodInfoPtr methodInfo) {
    if (orig_chat_Update) {
        orig_chat_Update(instance, methodInfo);
    }

    // Same strategy as the known-working ARM32 source: every chat update
    // forces the game's real TMP_InputField back to unlimited.
    ForceChatLimit(instance, "chat.Update");
}

// ============================================================================
// TMP_InputField hooks
// ============================================================================
static int my_TMP_GetCharacterLimit(void*, MethodInfoPtr) {
    return 0;
}

static void my_TMP_SetCharacterLimit(void* instance,
                                     int value,
                                     MethodInfoPtr) {
    LOGI("TMP.set_characterLimit(%d) -> 0 field=%p", value, instance);
    ForceUnlimitedInputField(instance, "TMP.set_characterLimit");
}

using StringInstanceFn = void (*)(void*, void*, MethodInfoPtr);
static StringInstanceFn orig_TMP_AppendString = nullptr;

static void my_TMP_AppendString(void* instance,
                                void* stringObject,
                                MethodInfoPtr methodInfo) {
    ForceUnlimitedInputField(instance, "TMP.Append(string)");
    if (orig_TMP_AppendString) {
        orig_TMP_AppendString(instance, stringObject, methodInfo);
    }
    ForceUnlimitedInputField(instance, "TMP.Append(string) AFTER");
}

static void my_TMP_AppendChar(void* instance,
                              uint32_t ch,
                              MethodInfoPtr methodInfo) {
    ForceUnlimitedInputField(instance, "TMP.Append(char)");
    if (orig_TMP_AppendChar) {
        orig_TMP_AppendChar(instance, ch, methodInfo);
    }
    ForceUnlimitedInputField(instance, "TMP.Append(char) AFTER");
}

static void my_TMP_InsertChar(void* instance,
                              uint32_t ch,
                              MethodInfoPtr methodInfo) {
    ForceUnlimitedInputField(instance, "TMP.Insert(char)");
    if (orig_TMP_InsertChar) {
        orig_TMP_InsertChar(instance, ch, methodInfo);
    }
    ForceUnlimitedInputField(instance, "TMP.Insert(char) AFTER");
}

static void my_TMP_ActivateInputFieldInternal(void* instance,
                                              MethodInfoPtr methodInfo) {
    ForceUnlimitedInputField(instance, "TMP.Activate BEFORE");
    if (orig_TMP_ActivateInputFieldInternal) {
        orig_TMP_ActivateInputFieldInternal(instance, methodInfo);
    }
    ForceUnlimitedInputField(instance, "TMP.Activate AFTER");
}

static void my_TMP_UpdateTouchKeyboard(void* instance,
                                       MethodInfoPtr methodInfo) {
    ForceUnlimitedInputField(instance, "TMP.UpdateTouchKeyboard BEFORE");
    if (orig_TMP_UpdateTouchKeyboard) {
        orig_TMP_UpdateTouchKeyboard(instance, methodInfo);
    }
    ForceUnlimitedInputField(instance, "TMP.UpdateTouchKeyboard AFTER");
}

// ============================================================================
// UIElements maxLength hooks
// ============================================================================
static int my_UIE_ITextEdition_GetMaxLength(void*, MethodInfoPtr) {
    return 0;
}

static void my_UIE_ITextEdition_SetMaxLength(void* instance,
                                             int value,
                                             MethodInfoPtr methodInfo) {
    LOGI("UIE.ITextEdition.set_maxLength(%d) -> 0", value);
    if (orig_UIE_ITextEdition_SetMaxLength) {
        orig_UIE_ITextEdition_SetMaxLength(instance, 0, methodInfo);
    }
}

static int my_UIE_GetMaxLength(void*, MethodInfoPtr) {
    return 0;
}

// ============================================================================
// TouchScreenKeyboard hooks
// ============================================================================
static void my_TSK_SetCharacterLimit(void* instance,
                                     int value,
                                     MethodInfoPtr methodInfo) {
    LOGI("TSK.set_characterLimit(%d) -> 0", value);
    if (orig_TSK_SetCharacterLimit) {
        orig_TSK_SetCharacterLimit(instance, 0, methodInfo);
    }
}

static void my_TSK_SetCharacterLimitInjected(void* unitySelf,
                                             int value,
                                             MethodInfoPtr methodInfo) {
    LOGI("TSK.set_characterLimit_Injected(%d) -> 0", value);
    if (orig_TSK_SetCharacterLimit_Injected) {
        orig_TSK_SetCharacterLimit_Injected(unitySelf, 0, methodInfo);
    }
}

// ============================================================================
// Hook setup
// ============================================================================
static void* hack_thread(void*) {
    LOGI("=== CursedHouseChat 64-bit ARM64 START ===");
    LOGI("Dump: chat.inputField=+0x30, TMP.m_CharacterLimit=+0x19C");
    LOGI("ARM64 address rule: loadBias + RVA. No +1 / no -0x10000.");

    while (g_il2cppLoadBias == 0) {
        g_il2cppLoadBias = GetModuleLoadBias("libil2cpp.so");
        if (g_il2cppLoadBias == 0) sleep(1);
    }

    LOGI("libil2cpp loadBias=0x%" PRIxPTR, g_il2cppLoadBias);

    Install64Hook(kChat_Update_RVA,
                  reinterpret_cast<void*>(my_chat_Update),
                  reinterpret_cast<void**>(&orig_chat_Update),
                  "chat.Update");

    // TMP limit + direct text input routes.
    Install64Hook(kTMP_GetCharacterLimit_RVA,
                  reinterpret_cast<void*>(my_TMP_GetCharacterLimit),
                  reinterpret_cast<void**>(&orig_TMP_GetCharacterLimit),
                  "TMP.get_characterLimit");

    Install64Hook(kTMP_SetCharacterLimit_RVA,
                  reinterpret_cast<void*>(my_TMP_SetCharacterLimit),
                  reinterpret_cast<void**>(&orig_TMP_SetCharacterLimit),
                  "TMP.set_characterLimit");

    Install64Hook(kTMP_AppendString_RVA,
                  reinterpret_cast<void*>(my_TMP_AppendString),
                  reinterpret_cast<void**>(&orig_TMP_AppendString),
                  "TMP.Append(string)");

    Install64Hook(kTMP_AppendChar_RVA,
                  reinterpret_cast<void*>(my_TMP_AppendChar),
                  reinterpret_cast<void**>(&orig_TMP_AppendChar),
                  "TMP.Append(char)");

    Install64Hook(kTMP_InsertChar_RVA,
                  reinterpret_cast<void*>(my_TMP_InsertChar),
                  reinterpret_cast<void**>(&orig_TMP_InsertChar),
                  "TMP.Insert(char)");

    Install64Hook(kTMP_ActivateInputFieldInternal_RVA,
                  reinterpret_cast<void*>(my_TMP_ActivateInputFieldInternal),
                  reinterpret_cast<void**>(&orig_TMP_ActivateInputFieldInternal),
                  "TMP.ActivateInputFieldInternal");

    Install64Hook(kTMP_UpdateTouchKeyboard_RVA,
                  reinterpret_cast<void*>(my_TMP_UpdateTouchKeyboard),
                  reinterpret_cast<void**>(&orig_TMP_UpdateTouchKeyboard),
                  "TMP.UpdateTouchKeyboardFromEditChanges");

    // UIElements fallback.
    Install64Hook(kUIE_ITextEdition_GetMaxLength_RVA,
                  reinterpret_cast<void*>(my_UIE_ITextEdition_GetMaxLength),
                  reinterpret_cast<void**>(&orig_UIE_ITextEdition_GetMaxLength),
                  "UIE.ITextEdition.get_maxLength");

    Install64Hook(kUIE_ITextEdition_SetMaxLength_RVA,
                  reinterpret_cast<void*>(my_UIE_ITextEdition_SetMaxLength),
                  reinterpret_cast<void**>(&orig_UIE_ITextEdition_SetMaxLength),
                  "UIE.ITextEdition.set_maxLength");

    Install64Hook(kUIE_GetMaxLength_RVA,
                  reinterpret_cast<void*>(my_UIE_GetMaxLength),
                  reinterpret_cast<void**>(&orig_UIE_GetMaxLength),
                  "UIE.get_maxLength");

    // TouchScreenKeyboard limit setters.
    Install64Hook(kTSK_SetCharacterLimit_RVA,
                  reinterpret_cast<void*>(my_TSK_SetCharacterLimit),
                  reinterpret_cast<void**>(&orig_TSK_SetCharacterLimit),
                  "TSK.set_characterLimit");

    Install64Hook(kTSK_SetCharacterLimit_Injected_RVA,
                  reinterpret_cast<void*>(my_TSK_SetCharacterLimitInjected),
                  reinterpret_cast<void**>(&orig_TSK_SetCharacterLimit_Injected),
                  "TSK.set_characterLimit_Injected");

    LOGI("=== CursedHouseChat 64-bit ALL HOOKS INSTALLED ===");
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
