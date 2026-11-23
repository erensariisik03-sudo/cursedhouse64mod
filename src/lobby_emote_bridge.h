#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <utility>
// Only native values cross the render/Unity boundary. Never store managed
// pointers or invoke Unity/Photon methods from an ImGui button callback.
namespace lobby_emote_bridge {
struct Snapshot {
    bool visible=false,ready=false;
    uint64_t generation=0;
    std::vector<std::string> ids;
    std::string status;
};
struct Command { uint64_t generation=0;std::string id;bool pending=false; };
inline std::mutex mutex;
// The opener is a native UI feature, independent of successful scene discovery.
inline Snapshot state{true,false,0,{},u8"v19 | Dans hedefi bekleniyor."};
inline Command command;
inline Snapshot Read(){std::lock_guard<std::mutex> lock(mutex);return state;}
inline void Publish(Snapshot value){
    std::lock_guard<std::mutex> lock(mutex);
    if(!value.visible||!value.ready||value.generation!=state.generation)command={};
    state=std::move(value);
}
inline void Request(uint64_t generation,const std::string& id){
    std::lock_guard<std::mutex> lock(mutex);
    if(!state.visible||!state.ready||generation!=state.generation||command.pending)return;
    command={generation,id,true};
}
inline Command Take(){std::lock_guard<std::mutex> lock(mutex);Command result=std::move(command);command={};return result;}
}
