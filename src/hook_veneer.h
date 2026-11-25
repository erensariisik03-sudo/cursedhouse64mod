#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace hook_veneer {
using Read = bool (*)(uintptr_t address, void* output, size_t size, void* context);
struct Result { uintptr_t address; unsigned hops; };
inline int64_t Signed(uint32_t value, unsigned bits) {
    const int64_t sign = int64_t{1} << (bits-1);
    return (static_cast<int64_t>(value) ^ sign) - sign;
}
// Follow only unconditional AArch64 B instructions; calls and conditional
// branches do not preserve the tail-branch contract required by the observer.
inline std::optional<Result> Resolve(uintptr_t entry, Read read, void* context) {
    std::array<uintptr_t,16> visited{};
    for (unsigned hop=0; hop<visited.size(); ++hop) {
        if (!entry || (entry&3)) return {};
        for (unsigned i=0;i<hop;++i) if (visited[i]==entry) return {};
        visited[hop]=entry;
        uint32_t instruction=0;
        if (!read(entry,&instruction,sizeof(instruction),context)) return {};
        if ((instruction&0xFC000000u)!=0x14000000u) return Result{entry,hop};
        const int64_t delta=Signed(instruction&0x03FFFFFFu,26)*4;
        entry=static_cast<uintptr_t>(static_cast<int64_t>(entry)+delta);
    }
    return {};
}
inline bool Overlaps(uintptr_t patch, size_t bytes, uintptr_t protected_entry) {
    return protected_entry && protected_entry>=patch && protected_entry-patch<bytes;
}
}
