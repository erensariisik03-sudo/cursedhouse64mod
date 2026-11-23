#include "room_vote.h"
#include <cassert>
#include <iostream>
#include <limits>
int main(){
    using namespace room_vote;
    const std::vector<Player> players={{1,"Host"},{2,"Same name"},{3,"Same name"}};
    Session s;assert(!s.Start(1,1,"Host",10,players,0));
    assert(s.Start(1,2,"Same name",10,players,0)&&s.yes==1&&s.no==0);
    assert(!s.Start(1,3,"Other",20,players,0)&&s.target==2);
    s.Open(5,100);assert(s.Remaining(5)==10&&s.Remaining(14.5)==1);
    assert(!s.Cast(1,false,6,101)); // initiating host already voted yes
    assert(!s.Cast(4,true,6,101)); // not enrolled
    assert(!s.Cast(2,false,6,99)); // cached/replayed message before this ballot
    assert(s.Cast(2,false,6,101)&&s.yes==s.no&&!s.KickWins());
    assert(!s.Cast(2,true,7,102)); // one immutable vote per authenticated actor
    assert(s.Cast(3,true,7,102)&&s.KickWins()); // identical nicknames remain distinct actors
    assert(!s.Cast(3,false,15,110));
    s.Clear();assert(!s.Active()&&!s.Cast(2,true,0,0));
    assert(s.Start(1,2,"Target",0,players,0)&&s.duration==1);s.Open(0,100);
    assert(s.KickWins()&&!s.Cast(2,false,1,101)); // only automatic host vote, deadline is exclusive
    s.Clear();assert(s.Start(1,2,"Target",600,players,0)&&s.duration==60);
    s.Open(0,std::numeric_limits<int32_t>::max()-2);
    assert(s.Cast(2,false,1,std::numeric_limits<int32_t>::min()+2)); // server timestamp wrap
    assert(!s.KickWins());s.Clear();
    assert(!s.Start(1,9,"Missing",10,players,0)&&!s.Active());
    std::cout<<"Room vote: automatic host yes, immutable actor votes, duplicate names, stale/replayed votes, deadline/tie rules, 1-60s bounds, cancellation and server-clock wrap: PASS\n";
}
