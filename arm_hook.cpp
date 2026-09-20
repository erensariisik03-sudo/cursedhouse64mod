#include "arm_hook.h"

#include <android/log.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

#define LOG_TAG "CursedHouseChat"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#if !defined(__arm__)
#error "arm_hook.cpp is intended for 32-bit ARM only"
#endif

namespace {

constexpr std::size_t kPatchSize = 8; // two ARM instructions

// ARM: LDR PC, [PC, #-4]
// At address A this reads the literal at A+4, so the next word is the
// absolute branch target.
constexpr uint32_t kAbsBranchInsn = 0xE51FF004u;

static void WriteAbsoluteBranch(uint8_t* dst, uintptr_t target) {
    uint32_t words[2] = {kAbsBranchInsn, static_cast<uint32_t>(target)};
    std::memcpy(dst, words, sizeof(words));
}

static bool MakeWritable(void* address, std::size_t size, int* oldProtOut) {
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) return false;
    const uintptr_t start = reinterpret_cast<uintptr_t>(address) & ~(static_cast<uintptr_t>(pageSize) - 1u);
    const uintptr_t end = (reinterpret_cast<uintptr_t>(address) + size + pageSize - 1u) & ~(static_cast<uintptr_t>(pageSize) - 1u);

    // We only need executable memory to be writable while installing the hook.
    // Restoring RX is best-effort; the original mapping permissions are not
    // required for the subsequent execution.
    (void)oldProtOut;
    if (mprotect(reinterpret_cast<void*>(start), end - start, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("mprotect(RWX) failed errno=%d", errno);
        return false;
    }
    return true;
}

}

int ArmHook(void* targetVoid, void* replacement, void** original) {
    if (!targetVoid || !replacement || !original) return -1;

    const uintptr_t target = reinterpret_cast<uintptr_t>(targetVoid) & ~static_cast<uintptr_t>(1u);
    const uintptr_t repl = reinterpret_cast<uintptr_t>(replacement);

    if ((target & 3u) != 0u) {
        LOGE("ArmHook target is not ARM-aligned: %p", reinterpret_cast<void*>(target));
        return -2;
    }

    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) return -3;

    // Trampoline: copy two original ARM instructions, then absolute-branch to
    // the remainder of the original function. The chat.Update target used by
    // this project starts with two non-PC-relative instructions, making this
    // minimal trampoline suitable for that target.
    const std::size_t trampolineSize = 16;
    void* tramp = mmap(nullptr, trampolineSize,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tramp == MAP_FAILED) {
        LOGE("mmap trampoline failed errno=%d", errno);
        return -4;
    }

    uint8_t* trampBytes = reinterpret_cast<uint8_t*>(tramp);
    std::memcpy(trampBytes, reinterpret_cast<const void*>(target), kPatchSize);
    WriteAbsoluteBranch(trampBytes + kPatchSize, target + kPatchSize);
    __builtin___clear_cache(reinterpret_cast<char*>(trampBytes), reinterpret_cast<char*>(trampBytes + trampolineSize));

    if (!MakeWritable(reinterpret_cast<void*>(target), kPatchSize, nullptr)) {
        munmap(tramp, trampolineSize);
        return -5;
    }

    uint8_t patch[kPatchSize];
    WriteAbsoluteBranch(patch, repl);
    std::memcpy(reinterpret_cast<void*>(target), patch, sizeof(patch));
    __builtin___clear_cache(reinterpret_cast<char*>(target), reinterpret_cast<char*>(target + kPatchSize));

    *original = tramp;
    LOGI("ArmHook installed target=%p replacement=%p trampoline=%p",
         reinterpret_cast<void*>(target), replacement, tramp);
    return 0;
}
