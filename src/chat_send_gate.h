#pragma once
#include <atomic>
namespace chat_send_gate {
// True only after the local ChatManager.SendMessage hook was installed.
inline std::atomic<bool> ready{false};
inline bool Available(bool game_allowed) {
    return game_allowed||ready.load(std::memory_order_acquire);
}
}
