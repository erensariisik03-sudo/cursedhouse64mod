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
Object* game_object=reinterpret_cast<Object*>(0x800);
Object* iterator_owner=pawn;
Field* view_field=reinterpret_cast<Field*>(0x10);
Field* iterator_field=reinterpret_cast<Field*>(0x20);
Field* scalar_field=reinterpret_cast<Field*>(0x30);
int scene=7,forwards=0,moves=0,vm_calls=0,writes=0;bool owned=true;
gameplay::Scene scene_box{7};
bool valid_scene=true,loaded_scene=true,object_alive=true,hierarchy=true,script_enabled=false,has_cached_view=true;
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
Class* ObjectClass(Object* o){assert(o==pawn);return reinterpret_cast<Class*>(0x900);}
Field* FindField(Class*,const char* name){return has_cached_view&&!std::strcmp(name,"qsa")?view_field:nullptr;}
Object* Invoke(const Method* method,void* self,void** args,Object** error){
 ++vm_calls;*error=nullptr;
 if(method==M(1)){scene_box.handle=scene;return reinterpret_cast<Object*>(&scene_box);}
 if(method==M(2)){assert(self==view);return reinterpret_cast<Object*>(&owned);}
 if(method==M(3))return room;
 if(method==M(4)){assert(args&&*static_cast<int*>(args[0])==scene);return reinterpret_cast<Object*>(&valid_scene);}
 if(method==M(5)){assert(args&&*static_cast<int*>(args[0])==scene);return reinterpret_cast<Object*>(&loaded_scene);}
 if(method==M(6)){assert(self==pawn);return view;}
 if(method==M(7)){assert(args&&args[0]);return reinterpret_cast<Object*>(&object_alive);}
 if(method==M(8)){assert(self==pawn);return game_object;}
 if(method==M(9)){assert(self==game_object);return reinterpret_cast<Object*>(&hierarchy);}
 if(method==M(10)){assert(self==pawn);return reinterpret_cast<Object*>(&script_enabled);}
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
 api.object_get_class=ObjectClass;api.class_get_field_from_name=FindField;
 gameplay::hook_api=&api;gameplay::unity_thread=std::this_thread::get_id();
 gameplay::scene_method=M(1);gameplay::mine_method=M(2);gameplay::room_method=M(3);gameplay::player_view=view_field;
 gameplay::scene_valid_method=M(4);gameplay::scene_loaded_method=M(5);gameplay::view_method=M(6);
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
  gameplay::protect=true;scene=-42;gameplay::protected_scene=scene;
  gameplay::AiDeath(pawn,nullptr);assert(forwards==11); // a negative loaded scene is valid
  valid_scene=false;gameplay::AiDeath(pawn,nullptr);assert(forwards==12);valid_scene=true;
  loaded_scene=false;gameplay::AiDeath(pawn,nullptr);assert(forwards==13);loaded_scene=true;
  gameplay::player_view=nullptr;gameplay::AiDeath(pawn,nullptr);assert(forwards==13); // inherited PhotonView getter
  owned=false;gameplay::NamedDeath(pawn,killer,nullptr);assert(forwards==14&&forwarded_name==killer);owned=true;
  gameplay::player_view=view_field;gameplay::protect=false;scene=7;
 }
 assert(roots.empty());
 {
  gameplay::Runtime runtime(api);gameplay::Actor actor(api,pawn,0);
  runtime.alive=M(7);runtime.go=M(8);runtime.go_active=M(9);runtime.active=M(10);runtime.mine=M(2);runtime.view_get=M(6);
  assert(!runtime.Active(pawn)&&runtime.InHierarchy(pawn)&&runtime.Owned(pawn,"qsa")); // disabled script, live owned object
  has_cached_view=false;assert(runtime.Owned(pawn,"qsa")); // Awake/Start has not filled the private cache
  owned=false;assert(!runtime.Owned(pawn,"qsa"));owned=true;
  hierarchy=false;assert(!runtime.InHierarchy(pawn));hierarchy=true;has_cached_view=true;
  int actual_scene=0;scene=-42;
  assert(gameplay::ReadScene(api,M(1),M(4),M(5),actual_scene)&&actual_scene==-42);
  scene=7;valid_scene=false;assert(!gameplay::ReadScene(api,M(1),M(4),M(5),actual_scene));valid_scene=true;
  loaded_scene=false;assert(!gameplay::ReadScene(api,M(1),M(4),M(5),actual_scene));loaded_scene=true;
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
 std::cout<<"Gameplay: default original forwarding, own-player/room/scene/thread God guards, signed scene validity/loading, disabled-script discovery, owned PhotonView fallback, iterator handling, GC roots, non-compounding speed/restore, stale UI command rejection and bounded queue: PASS\n";
}
