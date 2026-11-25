#include "room_limit_bridge.h"
#include <cassert>
int main(){using namespace room_limit_bridge;Snapshot s;s.ready=s.in_room=s.is_host=true;s.count=4;s.generation=7;assert(Allowed(s,{7,255},true));assert(!Allowed(s,{7,3},true));assert(!Allowed(s,{7,256},true));assert(!Allowed(s,{8,255},true));assert(!Allowed(s,{7,255},false));s.is_host=false;assert(!Allowed(s,{7,255},true));s.is_host=true;s.busy=true;assert(!Allowed(s,{7,255},true));s.busy=false;Publish(s);Apply(255,7);auto c=Take();assert(c.limit==255&&c.generation==7);assert(Take().limit==0);Apply(256,7);assert(Take().limit==0);}
