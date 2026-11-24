#pragma once
// Developer menu gate: fixed local verification. Not a remote authentication mechanism.
// A digest avoids a plaintext literal but does not make a distributed APK unbreakable.
#include "photo_transfer_security.h"
#include <array>
#include <string>
namespace developer_lock {
class Session {
    bool unlocked_ = false;
    unsigned failures_ = 0;
public:
    bool Unlocked() const { return unlocked_; }
    int AttemptsRemaining() const { return 5 - static_cast<int>(failures_); }
    bool Verify(const std::string& candidate) {
        if (failures_ >= 5 || candidate.empty() || candidate.size() > 128) return false;
        static constexpr std::array<uint8_t, 32> kDigest = {0x57, 0x94, 0xc2, 0xbd, 0x07, 0xe6, 0xe0, 0xa6, 0x2e, 0xb8, 0x0a, 0x77, 0x62, 0x3f, 0x10, 0xe0, 0x79, 0x34, 0xb7, 0x5b, 0x0a, 0x2b, 0xe5, 0x51, 0xec, 0x4b, 0x66, 0x36, 0x30, 0x96, 0x1e, 0xd9};
        const auto actual=photo_transfer_security::Hash(reinterpret_cast<const uint8_t*>(candidate.data()),candidate.size());
        const bool valid=photo_transfer_security::Same(actual.data(),kDigest.data(),kDigest.size());
        if (valid) { unlocked_=true; failures_=0; }
        else { ++failures_; unlocked_=false; }
        return valid;
    }
    void Lock() { unlocked_=false; }
};
} // namespace developer_lock
