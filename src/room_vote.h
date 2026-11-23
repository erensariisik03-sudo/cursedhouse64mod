#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>
namespace room_vote {
struct Player {int actor=0;std::string name;};
struct View {
    bool ready=false,active=false,counting=false;int target=0,yes=0,no=0,remaining=0,local=0;
    std::vector<Player> players;
    std::string status=u8"Şaka oylaması: host veya client başlatabilir.";
};
enum class Phase {Idle,Announce,Voting,Result,PrankDelay};
struct Session {
    Phase phase=Phase::Idle;int target=0,host=0,duration=10,yes=0,no=0;double deadline=0;
    int32_t opened_server=0;std::string name;
    std::unordered_set<int> eligible,voted;
    bool Active()const{return phase!=Phase::Idle;}
    void Clear(){*this=Session{};}
    bool Start(int owner,int victim,const std::string& nick,int seconds,const std::vector<Player>& players,double now){
        if(Active()||owner<=0||victim<=0||owner==victim||players.size()>128)return false;
        for(const auto& p:players)if(p.actor>0)eligible.insert(p.actor);
        if(!eligible.count(owner)||!eligible.count(victim)){Clear();return false;}
        host=owner;target=victim;name=nick;duration=std::clamp(seconds,1,60);yes=1;no=0;voted.insert(owner);
        phase=Phase::Announce;deadline=now+15;return true;
    }
    void Open(double now,int32_t server){phase=Phase::Voting;deadline=now+duration;opened_server=server;}
    bool Cast(int actor,bool approve,double now,int32_t sent_server){
        if(phase!=Phase::Voting||now>=deadline||int32_t(uint32_t(sent_server)-uint32_t(opened_server))<0||!eligible.count(actor)||voted.count(actor))return false;
        voted.insert(actor);if(approve)++yes;else ++no;return true;
    }
    bool KickWins()const{return yes>no;}
    int Remaining(double now)const{return phase==Phase::Voting?std::max(0,int(std::ceil(deadline-now))):0;}
};
}
