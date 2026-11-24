#pragma once
// No server-side bans. Commands are scoped to the current Photon room and
// verified locally against a per-device, explicitly configured shared key.
#include <mutex>
#include <string>
#include <utility>
#include <vector>
namespace kick_bridge {
struct Peer {int actor=0;std::string display;};
struct Snapshot {
    bool key_ready=false,in_room=false;
    int selected_actor=0;
    std::vector<Peer> peers;
    std::string status="Anahtar kontrol ediliyor...";
};
struct Commands {int choose_actor=0;bool kick=false,refresh=false;};
inline std::mutex mutex;
inline Snapshot state;
inline Commands pending;
inline Snapshot Read(){std::lock_guard<std::mutex> l(mutex);return state;}
inline void Publish(Snapshot s){std::lock_guard<std::mutex> l(mutex);state=std::move(s);}
inline Commands Take(){std::lock_guard<std::mutex> l(mutex);auto c=pending;pending={};return c;}
inline void ChooseActor(int id){std::lock_guard<std::mutex> l(mutex);pending.choose_actor=id;}
inline void Kick(){std::lock_guard<std::mutex> l(mutex);pending.kick=true;}
inline void Refresh(){std::lock_guard<std::mutex> l(mutex);pending.refresh=true;}
}
