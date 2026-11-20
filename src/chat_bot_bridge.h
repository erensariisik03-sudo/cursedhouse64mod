#pragma once
// Yalnız yerel değerler render/Unity sınırını geçer. ImGui düğmeleri Unity veya
// ağ çağrısı yapmaz; yalnız komut kuyruğuna yazar.
#include <mutex>
#include <string>
#include <utility>
#include <vector>
namespace chat_bot_bridge {
enum class Edit { None, Key, Persona };
struct Snapshot {
    bool chat_ready = false, enabled = false, has_key = false, busy = false;
    std::vector<std::string> players;
    std::vector<std::string> selected; bool all = false, strict = true; int mode = 0, words = 45;
    std::string model, status = u8"AI | Bekleniyor.", last_in, last_out, persona;
};
struct Command {
    bool toggle = false, reset_memory = false, refresh = false;
    bool select = false; std::string name;
    bool set_mode = false; int mode = 0; bool toggle_all = false, toggle_strict = false; bool set_words = false; int words = 45;
    Edit edit = Edit::None;
    bool Any() const { return toggle || reset_memory || refresh || select || set_mode || toggle_all || toggle_strict || set_words || edit != Edit::None; }
};
inline std::mutex mutex;
inline Snapshot state;
inline Command command;
inline Snapshot Read() { std::lock_guard<std::mutex> l(mutex); return state; }
inline void Publish(Snapshot s) { std::lock_guard<std::mutex> l(mutex); state = std::move(s); }
inline void Toggle() { std::lock_guard<std::mutex> l(mutex); command.toggle = !command.toggle; }
inline void Select(const std::string& n) { std::lock_guard<std::mutex> l(mutex); command.select = true; command.name = n; }
inline void SetMode(int m) { std::lock_guard<std::mutex> l(mutex); command.set_mode = true; command.mode = m; }
inline void ToggleAll() { std::lock_guard<std::mutex> l(mutex); command.toggle_all = !command.toggle_all; }
inline void ToggleStrict() { std::lock_guard<std::mutex> l(mutex); command.toggle_strict = !command.toggle_strict; }
inline void SetWords(int w) { std::lock_guard<std::mutex> l(mutex); command.set_words = true; command.words = w; }
inline void ResetMemory() { std::lock_guard<std::mutex> l(mutex); command.reset_memory = true; }
inline void Refresh() { std::lock_guard<std::mutex> l(mutex); command.refresh = true; }
inline void BeginEdit(Edit e) { std::lock_guard<std::mutex> l(mutex); command.edit = e; }
inline Command Take() { std::lock_guard<std::mutex> l(mutex); Command c = std::move(command); command = Command{}; return c; }
}
