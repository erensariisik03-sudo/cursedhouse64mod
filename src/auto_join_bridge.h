#pragma once
// Dolu odaya otomatik katılma köprüsü (UI <-> Unity thread). Yalnız yerel bayrak/dize taşır.
#include <mutex>
#include <string>
namespace auto_join_bridge {
struct Snapshot { bool offer = false, active = false; int attempts = 0; std::string room, status; };
struct Command { bool start = false, cancel = false; };
inline std::mutex mutex;
inline Snapshot state;
inline Command command;
inline Snapshot Read() { std::lock_guard<std::mutex> l(mutex); return state; }
inline void Publish(const Snapshot& s) { std::lock_guard<std::mutex> l(mutex); state = s; }
inline void Start() { std::lock_guard<std::mutex> l(mutex); command.start = true; }
inline void Cancel() { std::lock_guard<std::mutex> l(mutex); command.cancel = true; }
inline Command Take() { std::lock_guard<std::mutex> l(mutex); Command c = command; command = {}; return c; }
}
