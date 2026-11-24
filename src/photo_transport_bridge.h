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
    bool in_room=false;
    int local_actor=0;
    bool share_request=false, browsing=false, listing_ready=false, fetching=false;
    std::string share_request_from;
    int selected_actor=0;
    std::string selected_name,selected_file,selected_remote_file,status="Photon fotoğraf kanalı hazırlanıyor...";
    std::string incoming_from;
    std::vector<Peer> peers;
    std::vector<std::string> files,remote_files;
    size_t progress=0,total=0;
};
struct Commands {
    bool refresh=false,offer=false,accept=false,reject=false;
    bool request_browse=false,cancel_browse=false,allow_browse=false,deny_browse=false,fetch=false;
    int choose_actor=0;
    std::string choose_file,choose_remote_file;
};
inline std::mutex mutex;
inline Snapshot state;
inline Commands pending;
inline Snapshot Read(){std::lock_guard<std::mutex> l(mutex);return state;}
inline void Publish(Snapshot s){std::lock_guard<std::mutex> l(mutex);state=std::move(s);}
inline Commands Take(){std::lock_guard<std::mutex> l(mutex);Commands c=std::move(pending);pending={};return c;}
inline void RequestBrowse(){std::lock_guard<std::mutex> l(mutex);pending.request_browse=true;}
inline void CancelBrowse(){std::lock_guard<std::mutex> l(mutex);pending.cancel_browse=true;}
inline void AllowBrowse(){std::lock_guard<std::mutex> l(mutex);pending.allow_browse=true;}
inline void DenyBrowse(){std::lock_guard<std::mutex> l(mutex);pending.deny_browse=true;}
inline void ChooseRemoteFile(std::string name){std::lock_guard<std::mutex> l(mutex);pending.choose_remote_file=std::move(name);}
inline void Fetch(){std::lock_guard<std::mutex> l(mutex);pending.fetch=true;}
inline void Refresh(){std::lock_guard<std::mutex> l(mutex);pending.refresh=true;}
inline void ChooseActor(int actor){std::lock_guard<std::mutex> l(mutex);pending.choose_actor=actor;}
inline void ChooseFile(std::string filename){std::lock_guard<std::mutex> l(mutex);pending.choose_file=std::move(filename);}
inline void Offer(){std::lock_guard<std::mutex> l(mutex);pending.offer=true;}
inline void Accept(){std::lock_guard<std::mutex> l(mutex);pending.accept=true;}
inline void Reject(){std::lock_guard<std::mutex> l(mutex);pending.reject=true;}
}
