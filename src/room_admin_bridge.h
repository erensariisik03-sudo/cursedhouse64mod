#pragma once
// Developer-only room administration UI bridge. Commands are handled on the
// Unity/IL2CPP thread by photo_transport, never on the ImGui rendering thread.
#include <algorithm>
#include <mutex>
#include <string>
#include <utility>

namespace room_admin_bridge {
constexpr int kMinPlayers = 1;
// Photon Realtime's legacy byte-valued MaxPlayers supports 1..255. This is
// a protocol limit, not a promise that the game itself supports 255 players.
constexpr int kMaxPlayers = 255;
inline int ClampLimit(int value) {return std::max(kMinPlayers,std::min(kMaxPlayers,value));}
struct Snapshot {
    bool in_room=false, is_master=false, can_take_host=false, can_set_limit=false;
    bool can_use_local_host=false, local_host_enabled=false;
    int local_actor=0, master_actor=0, player_count=0;
    int max_players=0, selected_limit=20;
    std::string status="Oda yonetimi kontrol ediliyor...";
};
struct Commands {int desired_limit=0;bool take_host=false,apply_limit=false;
    bool set_local_host=false,local_host_value=false;};
inline std::mutex mutex;
inline Snapshot state;
inline Commands pending;
inline Snapshot Read(){std::lock_guard<std::mutex> lock(mutex);return state;}
inline void Publish(Snapshot value){std::lock_guard<std::mutex> lock(mutex);state=std::move(value);}
inline Commands Take(){std::lock_guard<std::mutex> lock(mutex);auto value=pending;pending={};return value;}
inline void ChooseLimit(int limit){std::lock_guard<std::mutex> lock(mutex);limit=ClampLimit(limit);state.selected_limit=limit;pending.desired_limit=limit;}
inline void ApplyLimit(){std::lock_guard<std::mutex> lock(mutex);pending.apply_limit=true;}
inline void TakeHost(){std::lock_guard<std::mutex> lock(mutex);pending.take_host=true;}
inline void SetLocalHost(bool on){std::lock_guard<std::mutex> lock(mutex);pending.set_local_host=true;pending.local_host_value=on;}
}
