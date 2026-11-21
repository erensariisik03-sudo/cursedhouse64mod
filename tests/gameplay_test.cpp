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
Object* attack_object=reinterpret_cast<Object*>(0xA00);
Object* state_object=reinterpret_cast<Object*>(0xB00);
Object* local_actor=reinterpret_cast<Object*>(0xC00);
Object* point_object=reinterpret_cast<Object*>(0xD00);
Object* iterator_owner=pawn;
Field* view_field=reinterpret_cast<Field*>(0x10);
Field* iterator_field=reinterpret_cast<Field*>(0x20);
Field* scalar_field=reinterpret_cast<Field*>(0x30);
Field* attack_field=reinterpret_cast<Field*>(0x40);
Field* state_field=reinterpret_cast<Field*>(0x50);
Field* drop_field=reinterpret_cast<Field*>(0x60);
int scene=7,forwards=0,moves=0,vm_calls=0,writes=0;bool owned=true;
gameplay::Scene scene_box{7};
bool valid_scene=true,loaded_scene=true,object_alive=true,hierarchy=true,script_enabled=false,has_cached_view=true;
float scalar=2.5f;
String* forwarded_name=nullptr;
int hit_forwards=0,state_forwards=0,ground_created=0;
uint8_t hand_item=0,last_entity=0,last_state=0;bool last_sync=false;
std::vector<uint8_t> dropped;
gameplay::Vec3 position{2,3,4},scale_value{.8f,.8f,.8f};gameplay::Quat rotation{0,0,0,1};
const Method* M(uintptr_t n){return reinterpret_cast<const Method*>(n);}
Handle NewRoot(Object* o,bool){roots[++next_root]=o;return next_root;}
Object* GetRoot(Handle h){return roots.at(h);}
void FreeRoot(Handle h){assert(roots.erase(h)==1);}
void ReadField(Object* o,Field* f,void* out){
 if(f==view_field){assert(o==pawn);*static_cast<Object**>(out)=view;}
 else if(f==iterator_field){assert(o==iterator);*static_cast<Object**>(out)=iterator_owner;}
 else if(f==scalar_field){assert(o==pawn);*static_cast<float*>(out)=scalar;}
 else if(f==attack_field){assert(o==attack_object);*static_cast<Object**>(out)=view;}
 else if(f==state_field){assert(o==state_object);*static_cast<Object**>(out)=view;}
 else if(f==drop_field){assert(o==pawn);*static_cast<Object**>(out)=point_object;}
 else assert(false);
}
void WriteField(Object* o,Field* f,void* value){assert(o==pawn&&f==scalar_field);scalar=*static_cast<float*>(value);++writes;}
Class* ObjectClass(Object* o){assert(o==pawn);return reinterpret_cast<Class*>(0x900);}
Field* FindField(Class*,const char* name){if(!std::strcmp(name,"dropPointTransform"))return drop_field;return has_cached_view&&(!std::strcmp(name,"qsa")||!std::strcmp(name,"pv"))?view_field:nullptr;}
String* NewString(const char*){return reinterpret_cast<String*>(0xF00);}
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
 if(method==M(11))return local_actor;
 if(method==M(12)){assert(self==pawn);return reinterpret_cast<Object*>(&hand_item);}
 if(method==M(13)){assert(self==pawn&&args&&args[0]);if(hand_item)dropped.push_back(hand_item);hand_item=*static_cast<uint8_t*>(args[0]);return nullptr;}
 if(method==M(14)){assert(self==point_object);return reinterpret_cast<Object*>(&position);}
 if(method==M(15)){assert(self==point_object);return reinterpret_cast<Object*>(&rotation);}
 if(method==M(16)||method==M(17))return reinterpret_cast<Object*>(0xF10);
 if(method==M(18)){assert(args&&args[0]&&args[1]&&args[2]&&args[3]);++ground_created;return reinterpret_cast<Object*>(0xF20);}
 if(method==M(19)){assert(self==pawn);return reinterpret_cast<Object*>(&script_enabled);}
 if(method==M(20)){assert(self==pawn);script_enabled=*static_cast<bool*>(args[0]);return nullptr;}
 if(method==M(21)){assert(self==pawn);return reinterpret_cast<Object*>(&scale_value);}
 if(method==M(22)){assert(self==pawn);scale_value=*static_cast<gameplay::Vec3*>(args[0]);return nullptr;}
 assert(false);return nullptr;
}
void* Unbox(Object* o){return o;}
void Original(Object*,const Method*){++forwards;}
void OriginalNamed(Object*,String* name,const Method*){++forwards;forwarded_name=name;}
bool OriginalMove(Object*,const Method*){++moves;return true;}
void OriginalHit(Object*,Object*,uint8_t entity,const Method*){++hit_forwards;last_entity=entity;}
void OriginalState(Object*,uint8_t value,bool sync,const Method*){++state_forwards;last_state=value;last_sync=sync;}
void OriginalSync(Object*,uint8_t value,const Method*){++state_forwards;last_state=value;}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
extern "C" int DobbyHook(void*,void*,void**){return -1;}
int main(){
 Api api{};api.gchandle_new=NewRoot;api.gchandle_get_target=GetRoot;api.gchandle_free=FreeRoot;
 api.runtime_invoke=Invoke;api.object_unbox=Unbox;api.field_get_value=ReadField;api.field_set_value=WriteField;
 api.object_get_class=ObjectClass;api.class_get_field_from_name=FindField;
 api.string_new=NewString;
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
  gameplay::Root attack(api,attack_object),state(api,state_object);
  gameplay::protected_attack=attack.h;gameplay::protected_state=state.h;
  gameplay::attack_view=attack_field;gameplay::state_view=state_field;gameplay::local_player_method=M(11);
  gameplay::protected_scene=scene;gameplay::protected_room=reinterpret_cast<uintptr_t>(room);
  gameplay::original_bat=OriginalHit;gameplay::original_eat=OriginalHit;gameplay::original_bite=OriginalHit;
  gameplay::original_bat_rpc=OriginalHit;gameplay::original_eat_rpc=OriginalHit;gameplay::original_bite_rpc=OriginalHit;
  gameplay::original_state_change=OriginalState;gameplay::original_state_sync=OriginalSync;
  gameplay::original_lava=Original;
  gameplay::protect=false;const int before=vm_calls;
  gameplay::Bat(attack_object,local_actor,2,nullptr);assert(hit_forwards==1&&last_entity==2&&vm_calls==before);
  gameplay::protect=true;
  gameplay::Bat(attack_object,local_actor,0,nullptr);gameplay::EatRpc(attack_object,local_actor,2,nullptr);
  assert(hit_forwards==1);
  gameplay::BiteRpc(attack_object,other,3,nullptr);assert(hit_forwards==2&&last_entity==3);
  gameplay::BatRpc(other,local_actor,1,nullptr);assert(hit_forwards==3&&last_entity==1);
  gameplay::ChangeState(state_object,1,true,nullptr);gameplay::SyncState(state_object,1,nullptr);assert(state_forwards==0);
  gameplay::ChangeState(state_object,2,false,nullptr);assert(state_forwards==1&&last_state==2&&!last_sync);
  owned=false;gameplay::ChangeState(state_object,1,true,nullptr);assert(state_forwards==2&&last_state==1&&last_sync);owned=true;
  ++scene;gameplay::Eat(attack_object,local_actor,2,nullptr);assert(hit_forwards==4);--scene;
  const int before_lava=forwards;gameplay::Lava(attack_object,nullptr);assert(forwards==before_lava);
  gameplay::protect=false;gameplay::Lava(attack_object,nullptr);assert(forwards==before_lava+1);
  gameplay::protected_attack=gameplay::protected_state=0;
 }
 assert(roots.empty());
 {
  gameplay::Runtime runtime(api);gameplay::Actor actor(api,pawn,0);
  runtime.alive=M(7);runtime.go=M(8);runtime.go_active=M(9);runtime.active=M(10);runtime.mine=M(2);runtime.view_get=M(6);
  runtime.pun_class=ObjectClass(pawn);
  assert(!runtime.Active(pawn)&&runtime.InHierarchy(pawn)&&runtime.Owned(pawn,"qsa")); // disabled script, live owned object
  has_cached_view=false;assert(runtime.Owned(pawn,"qsa")); // Awake/Start has not filled the private cache
  owned=false;assert(!runtime.Owned(pawn,"qsa"));owned=true;
  hierarchy=false;assert(!runtime.InHierarchy(pawn));hierarchy=true;has_cached_view=true;
  int actual_scene=0;scene=-42;
  assert(gameplay::ReadScene(api,M(1),M(4),M(5),actual_scene)&&actual_scene==-42);
  scene=7;valid_scene=false;assert(!gameplay::ReadScene(api,M(1),M(4),M(5),actual_scene));valid_scene=true;
  loaded_scene=false;assert(!gameplay::ReadScene(api,M(1),M(4),M(5),actual_scene));loaded_scene=true;
  runtime.enabled_get=M(19);runtime.enabled_set=M(20);runtime.scale_get=M(21);runtime.scale_set=M(22);
  actor.animations.push_back(std::make_unique<gameplay::AnimationNode>(api,pawn));
  script_enabled=true;runtime.FreezeAnimations(actor,true);assert(!script_enabled);
  script_enabled=true;runtime.FreezeAnimations(actor,true);assert(!script_enabled); // another script tried to restart it
  runtime.FreezeAnimations(actor,false);assert(script_enabled);
  script_enabled=false;runtime.FreezeAnimations(actor,true);runtime.FreezeAnimations(actor,false);assert(!script_enabled);
  actor.visuals.push_back(std::make_unique<gameplay::ScaleNode>(api,pawn));
  runtime.ScaleVisuals(actor,20);assert(gameplay::Near(scale_value.x,16));
  runtime.ScaleVisuals(actor,20);assert(gameplay::Near(scale_value.x,16));
  runtime.ScaleVisuals(actor,1);assert(gameplay::Near(scale_value.x,.8f));
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
 {
  gameplay::Runtime runtime(api);runtime.inventory.Set(pawn);runtime.snap.spawn_ready=true;
  runtime.alive=M(7);runtime.go=M(8);runtime.go_active=M(9);runtime.mine=M(2);runtime.view_get=M(6);
  runtime.hand_get=M(12);runtime.hand_pick=M(13);runtime.pos_get=M(14);runtime.rot_get=M(15);runtime.prefix_get=M(16);runtime.path_combine=M(17);runtime.spawn=M(18);
  for(int id:{1,2,48})runtime.prefabs.push_back(std::make_unique<gameplay::Prefab>(api,id,game_object,"Prefab"));
  runtime.Spawn(1);assert(hand_item==1&&dropped.empty()&&ground_created==0);
  runtime.Spawn(1);assert(hand_item==1&&dropped.empty()&&ground_created==1);
  runtime.Spawn(2);assert(hand_item==2&&dropped==std::vector<uint8_t>{1}&&ground_created==1);
  runtime.Spawn(48);assert(hand_item==2&&dropped.size()==1&&ground_created==2); // ammunition never replaces the held item
 }
 assert(roots.empty());
 using namespace gameplay_bridge;
 Clear();Snapshot s;s.ready=true;s.context=10;Publish(s);
 Command old{Kind::Spawn,9};assert(!Queue(old));
 Command tune{Kind::EnemySettings,10};tune.settings.speed=std::numeric_limits<float>::quiet_NaN();assert(!Queue(tune));
 tune.settings.speed=20;assert(Queue(tune));tune.settings.speed=2;assert(Queue(tune));
 auto q=Take();assert(q.size()==1&&q.front().settings.speed==2);
 tune.enemy=4;tune.settings.size=50;assert(Queue(tune));q=Take();assert(q.front().settings.size==20);
 s.context=11;Publish(s);assert(!Queue(tune));
 Command spawn{Kind::Spawn,11};for(int i=0;i<24;++i)assert(Queue(spawn));assert(!Queue(spawn));Clear();
 std::cout<<"Gameplay: legacy/modern own-player God guards, other-target and non-dead state forwarding, signed scene validity/loading, disabled-script discovery, safe PhotonView fallback, whole-animation freeze/restore, 20x visual scale, first/same/different hand-item flow and ground ammo, iterator handling, GC roots, non-compounding speed/restore, stale UI commands and bounded queue: PASS\n";
}
