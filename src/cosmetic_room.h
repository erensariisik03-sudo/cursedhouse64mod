#pragma once
#include "il2cpp_runtime.h"
#include <array>
#include <optional>
#include <string>
#include <cstdint>
namespace cosmetic_room {
// Own-player properties only. A successful invocation is not a server receipt.
class Plan {
    std::string account_;
    uintptr_t player_=0, room_=0;
    bool connected_=false;
    std::array<std::optional<std::string>,2> wanted_{};
    std::array<unsigned,2> attempts_{};
    std::array<bool,2> invoked_{};
    std::array<double,2> next_{};
public:
    void Context(const std::string& account,bool connected,uintptr_t player,uintptr_t room) {
        if (account!=account_) { account_=account;wanted_={};attempts_={};invoked_={};next_={}; }
        if (connected!=connected_ || player!=player_ || room!=room_) { attempts_={};invoked_={};next_={}; }
        connected_=connected;player_=player;room_=room;
    }
    void Select(size_t category,const std::string& id) {
        if (account_.empty() || category>=2 || (category==1 && id.empty())) return;
        if (!wanted_[category] || *wanted_[category]!=id) {
            wanted_[category]=id;attempts_[category]=0;invoked_[category]=false;next_[category]=0;
        }
    }
    const std::optional<std::string>& Wanted(size_t category) const { return wanted_[category]; }
    bool Due(size_t category,double now,std::optional<bool> local_match) const {
        return connected_ && player_ && !account_.empty() && wanted_[category] &&
            now>=next_[category] && attempts_[category]<3 &&
            (!invoked_[category] || local_match==false);
    }
    void Attempt(size_t category,double now,bool invoked) {
        ++attempts_[category];invoked_[category]=invoked;next_[category]=now+1.0;
    }
};
void Pump(const il2cpp::Api&,bool paused=false);
}
