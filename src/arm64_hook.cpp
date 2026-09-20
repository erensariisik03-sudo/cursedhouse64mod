#include "arm64_hook.h"

#include <android/log.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

#define LOG_TAG "CursedHouseChat"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#if !defined(__aarch64__)
#error "arm64_hook.cpp is intended for AArch64/arm64-v8a only"
#endif

namespace {

constexpr std::size_t kPatchSize = 16;
constexpr std::size_t kJumpSize = 16;
constexpr std::size_t kTrampolineSize = 32;

// AArch64:
//   ldr x16, #8
//   br  x16
//   .quad target
//
// 0x58000050 = LDR X16, #8
// 0xD61F0200 = BR X16
static void WriteAbsoluteJump(uint8_t* dst, uintptr_t target) {
    const uint32_t code[2] = {
        0x58000050u,
        0xD61F0200u
    };

    std::memcpy(dst, code, sizeof(code));
    const uint64_t absolute = static_cast<uint64_t>(target);
    std::memcpy(dst + 8, &absolute, sizeof(absolute));
}

static bool MakeWritable(void* address, std::size_t size) {
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) return false;

    const uintptr_t pageMask = static_cast<uintptr_t>(pageSize - 1);
    const uintptr_t begin = reinterpret_cast<uintptr_t>(address) & ~pageMask;
    const uintptr_t end =
        (reinterpret_cast<uintptr_t>(address) + size + pageMask) & ~pageMask;

    if (mprotect(reinterpret_cast<void*>(begin), end - begin,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("Arm64Hook: mprotect(RWX) failed errno=%d", errno);
        return false;
    }

    return true;
}

static void FlushInstructionCache(void* address, std::size_t size) {
    auto* begin = reinterpret_cast<char*>(address);
    __builtin___clear_cache(begin, begin + size);
}

} // namespace

int Arm64Hook(void* targetVoid, void* replacement, void** original) {
    if (!targetVoid || !replacement || !original) return -1;

    const uintptr_t target = reinterpret_cast<uintptr_t>(targetVoid);
    const uintptr_t repl = reinterpret_cast<uintptr_t>(replacement);

    // AArch64 instructions are always 4-byte aligned.
    if ((target & 0x3u) != 0u) {
        LOGE("Arm64Hook: target is not 4-byte aligned: %p", targetVoid);
        return -2;
    }

    void* trampoline = mmap(nullptr,
                            kTrampolineSize,
                            PROT_READ | PROT_WRITE | PROT_EXEC,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1,
                            0);
    if (trampoline == MAP_FAILED) {
        LOGE("Arm64Hook: mmap trampoline failed errno=%d", errno);
        return -3;
    }

    uint8_t* tramp = reinterpret_cast<uint8_t*>(trampoline);

    // NOTE:
    // The first 16 bytes of the target are copied verbatim. This is safe for
    // normal IL2CPP prologues made from STP/MOV/SUB/LDR/STR instructions.
    // PC-relative instructions in these first 16 bytes would require relocation.
    std::memcpy(tramp, reinterpret_cast<const void*>(target), kPatchSize);
    WriteAbsoluteJump(tramp + kPatchSize, target + kPatchSize);
    FlushInstructionCache(tramp, kTrampolineSize);

    if (!MakeWritable(reinterpret_cast<void*>(target), kPatchSize)) {
        munmap(trampoline, kTrampolineSize);
        return -4;
    }

    uint8_t patch[kJumpSize] = {};
    WriteAbsoluteJump(patch, repl);
    std::memcpy(reinterpret_cast<void*>(target), patch, sizeof(patch));
    FlushInstructionCache(reinterpret_cast<void*>(target), kJumpSize);

    *original = trampoline;

    LOGI("Arm64Hook installed target=%p replacement=%p trampoline=%p",
         reinterpret_cast<void*>(target),
         replacement,
         trampoline);
    return 0;
}
