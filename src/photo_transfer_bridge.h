#pragma once
// Render-to-Unity bridge. Passwords pass through a one-shot command and are
// never placed in a published UI snapshot.
#include <mutex>
#include <string>
#include <utility>
namespace photo_transfer_bridge {
struct Snapshot { bool configured=false,unlocked=false,receiving=false,transport_ready=false;int attempts_left=5;std::string status="Fotoğraf aktarımı henüz ağ kanalına bağlanmadı."; };
struct Command { bool lock=false,toggle_receive=false; };
inline std::mutex mutex;
inline Snapshot state;
inline Command command;
inline Snapshot Read(){std::lock_guard<std::mutex> l(mutex);return state;}
inline void Publish(Snapshot s){std::lock_guard<std::mutex> l(mutex);state=std::move(s);}
inline Command Take(){std::lock_guard<std::mutex> l(mutex);auto c=command;command={};return c;}
inline void Lock(){std::lock_guard<std::mutex> l(mutex);command.lock=true;}
inline void ToggleReceive(){std::lock_guard<std::mutex> l(mutex);command.toggle_receive=true;}
}
