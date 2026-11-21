#pragma once
// Live wardrobe-panel bridge. Only copied strings/numbers cross from the Unity
// scripting thread to the render thread. The lists are read from the same fi
// getters that CosmeticsLoader.haz passes to CosmeticsLoader.hba to build cards;
// they are not copied from the raw ey account-profile lists.
#include <algorithm>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
namespace wardrobe_bridge {
// category: 0 = aksesuar/şapka, 1 = karakter (skin) -- cosmetic_room ile aynı.
struct Snapshot {
    std::vector<std::string> items[2];
    std::string selected[2];
    std::string status;
    std::string diagnostics;
};
struct Request { bool pending = false; size_t category = 0; std::string id; };
inline std::mutex mutex;
inline Snapshot state;
inline Request request;
inline bool rejoin_requested = false;
inline bool probe_requested = false;
inline Snapshot Read() { std::lock_guard<std::mutex> l(mutex); return state; }
inline void SetWardrobeLists(std::vector<std::string> accessories, std::vector<std::string> skins) {
    std::lock_guard<std::mutex> l(mutex);
    state.items[0] = std::move(accessories); state.items[1] = std::move(skins);
}
inline void ClearLists() { std::lock_guard<std::mutex> l(mutex); state.items[0].clear(); state.items[1].clear(); request = {}; }
inline void SetSelected(size_t category, const std::string& id) { std::lock_guard<std::mutex> l(mutex); if (category < 2) state.selected[category] = id; }
// UI calls this: only IDs present in the live wardrobe snapshot are accepted.
inline void SetStatus(const std::string& text) { std::lock_guard<std::mutex> l(mutex); state.status = text; }
inline bool Select(size_t category, const std::string& id) {
    std::lock_guard<std::mutex> l(mutex);
    if (category >= 2) return false;
    if (id.empty()) { if (category != 0) return false; request = {true, 0, id}; return true; } // şapkasız: yalnız kendi şapkanı çıkarır
    const auto& list = state.items[category];
    if (std::find(list.begin(), list.end(), id) == list.end()) return false;
    request = {true, category, id}; return true;
}
inline void RequestRejoin() { std::lock_guard<std::mutex> l(mutex); rejoin_requested = true; }
inline bool TakeRejoin() { std::lock_guard<std::mutex> l(mutex); const bool r = rejoin_requested; rejoin_requested = false; return r; }
inline void RequestProbe() { std::lock_guard<std::mutex> l(mutex); probe_requested=true; state.diagnostics=u8"Ağ incelemesi için Unity karesi bekleniyor..."; }
inline bool TakeProbe() { std::lock_guard<std::mutex> l(mutex); const bool r=probe_requested; probe_requested=false; return r; }
inline void SetDiagnostics(const std::string& text) { std::lock_guard<std::mutex> l(mutex); state.diagnostics=text; }
inline Request Take() { std::lock_guard<std::mutex> l(mutex); Request r = std::move(request); request = {}; return r; }
}
