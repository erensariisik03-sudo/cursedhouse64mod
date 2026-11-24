#pragma once
// Yalnız yerel değerler render/Unity sınırını geçer. ImGui düğmeleri Unity veya
// ağ çağrısı yapmaz; yalnız komut kuyruğuna yazar.
#include <mutex>
#include <string>
#include <utility>
#include <vector>
namespace chat_bot_bridge {
enum class Edit { None, Key, Persona, Speech, Delegate, DeveloperUnlock };
struct Snapshot {
    bool chat_ready = false, enabled = false, has_key = false, busy = false, cool = true;
    bool developer_unlocked = false; int developer_attempts_left = 5;
    std::vector<std::string> players;
    std::vector<std::string> delegate_players; // Photon nicknames, unchanged including rich tags and Unicode
    std::vector<std::string> selected; bool all = false, strict = true, message_filter = true; int mode = 0, words = 45;
    // Most recent visible room messages, exactly as provided by the game's TMP label.
    // These stay local; no remote message-reading or invisible collection.
    std::vector<std::string> raw_messages;
    std::string model, status = u8"AI | Bekleniyor.", last_in, last_out, persona, room_name, delegate_name;
};
struct Command {
    bool toggle = false, set_enabled = false, enable = false, toggle_cool = false, voice_fx = false, voice_fx_on = false, reset_memory = false, refresh = false, lock_developer = false;
    bool select = false, select_delegate = false; std::string name, delegate_name;
    bool set_mode = false; int mode = 0; bool toggle_all = false, toggle_strict = false, toggle_filter = false; bool set_words = false; int words = 45;
    Edit edit = Edit::None;
    bool Any() const { return lock_developer || toggle || set_enabled || toggle_cool || voice_fx || select_delegate || reset_memory || refresh || select || set_mode || toggle_all || toggle_strict || toggle_filter || set_words || edit != Edit::None; }
};
inline std::mutex mutex;
inline Snapshot state;
inline Command command;
inline Snapshot Read() { std::lock_guard<std::mutex> l(mutex); return state; }
inline void Publish(Snapshot s) { std::lock_guard<std::mutex> l(mutex); state = std::move(s); }
inline void LockDeveloper() { std::lock_guard<std::mutex> l(mutex); command.lock_developer=true; }
inline void Toggle() { std::lock_guard<std::mutex> l(mutex); command.toggle = !command.toggle; }
inline void Enable(bool enabled) { std::lock_guard<std::mutex> l(mutex); command.set_enabled=true;command.enable=enabled;command.toggle=false; }
inline void ToggleCool() { std::lock_guard<std::mutex> l(mutex); command.toggle_cool=!command.toggle_cool; }
inline void VoiceFx(bool active) { std::lock_guard<std::mutex> l(mutex); command.voice_fx=true;command.voice_fx_on=active; }
inline void SelectDelegate(std::string name) { std::lock_guard<std::mutex> l(mutex);command.select_delegate=true;command.delegate_name=std::move(name); }
inline void Select(const std::string& n) { std::lock_guard<std::mutex> l(mutex); command.select = true; command.name = n; }
inline void SetMode(int m) { std::lock_guard<std::mutex> l(mutex); command.set_mode = true; command.mode = m; }
inline void ToggleAll() { std::lock_guard<std::mutex> l(mutex); command.toggle_all = !command.toggle_all; }
inline void ToggleStrict() { std::lock_guard<std::mutex> l(mutex); command.toggle_strict = !command.toggle_strict; }
inline void ToggleFilter() { std::lock_guard<std::mutex> l(mutex); command.toggle_filter = !command.toggle_filter; }
inline void SetWords(int w) { std::lock_guard<std::mutex> l(mutex); command.set_words = true; command.words = w; }
inline void ResetMemory() { std::lock_guard<std::mutex> l(mutex); command.reset_memory = true; }
inline void Refresh() { std::lock_guard<std::mutex> l(mutex); command.refresh = true; }
inline void BeginEdit(Edit e) { std::lock_guard<std::mutex> l(mutex); command.edit = e; }
inline Command Take() { std::lock_guard<std::mutex> l(mutex); Command c = std::move(command); command = Command{}; return c; }
}
