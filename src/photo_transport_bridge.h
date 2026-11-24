#pragma once
// Model->view bridge. Only the local user can choose an outgoing image or
// approve an incoming offer. Remote packets cannot select local paths.
#include <mutex>
#include <string>
#include <utility>
#include <vector>
namespace photo_transport_bridge {
struct Peer {int actor=0;std::string display;};
struct Snapshot {
    bool ready=false, incoming=false, sending=false;
    int selected_actor=0;
    std::string selected_name,selected_file,status="Photon fotoğraf kanalı hazırlanıyor...";
    std::string incoming_from;
    std::vector<Peer> peers;
    std::vector<std::string> files;
    size_t progress=0,total=0;
};
struct Commands {
    bool refresh=false,offer=false,accept=false,reject=false;
    int choose_actor=0;
    std::string choose_file;
};
inline std::mutex mutex;
inline Snapshot state;
inline Commands pending;
inline Snapshot Read(){std::lock_guard<std::mutex> l(mutex);return state;}
inline void Publish(Snapshot s){std::lock_guard<std::mutex> l(mutex);state=std::move(s);}
inline Commands Take(){std::lock_guard<std::mutex> l(mutex);Commands c=std::move(pending);pending={};return c;}
inline void Refresh(){std::lock_guard<std::mutex> l(mutex);pending.refresh=true;}
inline void ChooseActor(int actor){std::lock_guard<std::mutex> l(mutex);pending.choose_actor=actor;}
inline void ChooseFile(std::string filename){std::lock_guard<std::mutex> l(mutex);pending.choose_file=std::move(filename);}
inline void Offer(){std::lock_guard<std::mutex> l(mutex);pending.offer=true;}
inline void Accept(){std::lock_guard<std::mutex> l(mutex);pending.accept=true;}
inline void Reject(){std::lock_guard<std::mutex> l(mutex);pending.reject=true;}
}
