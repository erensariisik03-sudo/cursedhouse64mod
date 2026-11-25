#include "room_admin_bridge.h"
#include <cassert>
int main(){
 using namespace room_admin_bridge;
 assert(ClampLimit(-3)==1);
 assert(ClampLimit(4)==4);
 assert(ClampLimit(300)==255);
 ChooseLimit(300);auto cmd=Take();assert(cmd.desired_limit==255);
 assert(Read().selected_limit==255);
 TakeHost();ApplyLimit();SetLocalHost(true);cmd=Take();assert(cmd.take_host&&cmd.apply_limit&&cmd.set_local_host&&cmd.local_host_value);
 SetLocalHost(false);cmd=Take();assert(cmd.set_local_host&&!cmd.local_host_value);
 cmd=Take();assert(!cmd.take_host&&!cmd.apply_limit);
 Snapshot state;state.in_room=true;state.master_actor=2;state.local_actor=1;
 Publish(state);assert(Read().master_actor==2);
}
