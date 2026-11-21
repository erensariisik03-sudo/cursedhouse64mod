#include "../src/gameplay.cpp"
#include <cassert>
#include <iostream>
#include <map>
#include <limits>
using namespace il2cpp;
namespace {
std::map<Handle,Object*> roots;Handle next_root=0;
Object* pawn=reinterpret_cast<Object*>(0x100);
Object* owner_manager=reinterpret_cast<Object*>(0x200);
Object* view=reinterpret_cast<Object*>(0x300);
Object* room=reinterpret_cast<Object*>(0x400);
Object* iterator=reinterpret_cast<Object*>(0x500);
Object* other=reinterpret_cast<Object*>(0x600);
Object* iterator_owner=pawn;
Field* view_field=reinterpret_cast<Field*>(0x10);
Field* iterator_field=reinterpret_cast<Field*>(0x20);
Field* scalar_field=reinterpret_cast<Field*>(0x30);
int scene=7,forwards=0,moves=0,vm_calls=0,writes=0;bool owned=true;
float scalar=2.5f;
String* forwarded_name=nullptr;
const Method* M(uintptr_t n){return reinterpret_cast<const Method*>(n);}
Handle NewRoot(Object* o,bool){roots[++next_root]=o;return next_root;}
Object* GetRoot(Handle h){return roots.at(h);}
void FreeRoot(Handle h){assert(roots.erase(h)==1);}
void ReadField(Object* o,Field* f,void* out){
 if(f==view_field){assert(o==pawn);*static_cast<Object**>(out)=view;}
 else if(f==iterator_field){assert(o==iterator);*static_cast<Object**>(out)=iterator_owner;}
 else if(f==scalar_field){assert(o==pawn);*static_cast<float*>(out)=scalar;}
 else assert(false);
}
void WriteField(Object* o,Field* f,void* value){assert(o==pawn&&f==scalar_field);scalar=*static_cast<float*>(value);++writes;}
Object* Invoke(const Method* method,void* self,void**,Object** error){
 ++vm_calls;*error=nullptr;
 if(method==M(1))return reinterpret_cast<Object*>(&scene);
 if(method==M(2)){assert(self==view);return reinterpret_cast<Object*>(&owned);}
 if(method==M(3))return room;
 assert(false);return nullptr;
}
void* Unbox(Object* o){return o;}
void Original(Object*,const Method*){++forwards;}
void OriginalNamed(Object*,String* name,const Method*){++forwards;forwarded_name=name;}
bool OriginalMove(Object*,const Method*){++moves;return true;}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
extern "C" int DobbyHook(void*,void*,void**){return -1;}
int main(){
 Api api{};api.gchandle_new=NewRoot;api.gchandle_get_target=GetRoot;api.gchandle_free=FreeRoot;
 api.runtime_invoke=Invoke;api.object_unbox=Unbox;api.field_get_value=ReadField;api.field_set_value=WriteField;
 gameplay::hook_api=&api;gameplay::unity_thread=std::this_thread::get_id();
 gameplay::scene_method=M(1);gameplay::mine_method=M(2);gameplay::room_method=M(3);gameplay::player_view=view_field;
 gameplay::mom_owner=iterator_field;
 gameplay::original_die=Original;gameplay::original_ai=Original;gameplay::original_spider=Original;gameplay::original_spider_ai=Original;
 gameplay::original_named=OriginalNamed;gameplay::original_mom=OriginalMove;
 {
  gameplay::Root local(api,pawn),manager(api,owner_manager);
  gameplay::protected_player=local.h;gameplay::protected_manager=manager.h;
  gameplay::protected_scene=scene;gameplay::protected_room=reinterpret_cast<uintptr_t>(room);
  auto* killer=reinterpret_cast<String*>(0x700);
  gameplay::protect=false;
  gameplay::Die(owner_manager,nullptr);gameplay::AiDeath(pawn,nullptr);gameplay::SpiderDeath(pawn,nullptr);
  gameplay::SpiderAiDeath(pawn,nullptr);gameplay::NamedDeath(pawn,killer,nullptr);
  assert(gameplay::MomDeath(iterator,nullptr)&&forwards==5&&moves==1&&forwarded_name==killer&&vm_calls==0);
  gameplay::protect=true;
  gameplay::Die(owner_manager,nullptr);gameplay::AiDeath(pawn,nullptr);gameplay::SpiderDeath(pawn,nullptr);
  gameplay::SpiderAiDeath(pawn,nullptr);gameplay::NamedDeath(pawn,killer,nullptr);
  assert(!gameplay::MomDeath(iterator,nullptr)&&forwards==5&&moves==1&&roots.size()==2);
  gameplay::AiDeath(other,nullptr);iterator_owner=other;
  assert(gameplay::MomDeath(iterator,nullptr)&&forwards==6&&moves==2);iterator_owner=pawn;
  owned=false;gameplay::Die(owner_manager,nullptr);assert(forwards==7);owned=true;
  ++scene;gameplay::AiDeath(pawn,nullptr);assert(forwards==8);--scene;
  Object* saved_room=room;room=nullptr;gameplay::SpiderDeath(pawn,nullptr);assert(forwards==9);room=saved_room;
  const int before_vm=vm_calls;
  std::thread off_thread([]{gameplay::AiDeath(pawn,nullptr);});off_thread.join();
  assert(forwards==10&&vm_calls==before_vm&&roots.size()==2);
  gameplay::protect=false;gameplay::Die(owner_manager,nullptr);assert(forwards==11);
 }
 assert(roots.empty());
 {
  gameplay::Runtime runtime(api);gameplay::Actor actor(api,pawn,0);
  actor.speed_fields.push_back({scalar_field,scalar,scalar,false});
  runtime.ScalarApply(actor,actor.speed_fields,1);assert(writes==0&&scalar==2.5f);
  runtime.ScalarApply(actor,actor.speed_fields,2);assert(scalar==5.f);
  runtime.ScalarApply(actor,actor.speed_fields,2);assert(scalar==5.f); // no exponential amplification
  scalar=7;runtime.ScalarApply(actor,actor.speed_fields,3);assert(scalar==21.f);
  runtime.ScalarApply(actor,actor.speed_fields,1);assert(scalar==7.f);
  runtime.ScalarApply(actor,actor.speed_fields,2);scalar=4;const int before=writes;
  runtime.ScalarApply(actor,actor.speed_fields,1);assert(scalar==4&&writes==before); // preserve a newer game value
 }
 assert(roots.empty());
 using namespace gameplay_bridge;
 Clear();Snapshot s;s.ready=true;s.context=10;Publish(s);
 Command old{Kind::Spawn,9};assert(!Queue(old));
 Command tune{Kind::EnemySettings,10};tune.settings.speed=std::numeric_limits<float>::quiet_NaN();assert(!Queue(tune));
 tune.settings.speed=20;assert(Queue(tune));tune.settings.speed=2;assert(Queue(tune));
 auto q=Take();assert(q.size()==1&&q.front().settings.speed==2);
 s.context=11;Publish(s);assert(!Queue(tune));
 Command spawn{Kind::Spawn,11};for(int i=0;i<24;++i)assert(Queue(spawn));assert(!Queue(spawn));Clear();
 std::cout<<"Gameplay: default original forwarding, own-player/room/scene/thread God guards, iterator handling, GC roots, non-compounding speed/restore, stale UI command rejection and bounded queue: PASS\n";
}
