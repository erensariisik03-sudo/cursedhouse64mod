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
Object* script_object=reinterpret_cast<Object*>(0x1100);
Object* movement_object=reinterpret_cast<Object*>(0x1200);
Object* fall_object=reinterpret_cast<Object*>(0x1300);
Object* cross_object=reinterpret_cast<Object*>(0x1400);
Object* shot_object=reinterpret_cast<Object*>(0x1500);
Object* shot_manager_object=reinterpret_cast<Object*>(0x1600);
Object* body_controller=reinterpret_cast<Object*>(0x1700);
Object* body_transform=reinterpret_cast<Object*>(0x1800);
Object* child_array=reinterpret_cast<Object*>(0x1900);
Object* clone_view=reinterpret_cast<Object*>(0x1A00);
Object* explosion_object=reinterpret_cast<Object*>(0x1B00);
Object* remote_explosion=reinterpret_cast<Object*>(0x1C00);
Field* explosion_field=reinterpret_cast<Field*>(0x130);
Object* iterator_owner=pawn;
Field* view_field=reinterpret_cast<Field*>(0x10);
Field* iterator_field=reinterpret_cast<Field*>(0x20);
Field* scalar_field=reinterpret_cast<Field*>(0x30);
Field* attack_field=reinterpret_cast<Field*>(0x40);
Field* state_field=reinterpret_cast<Field*>(0x50);
Field* drop_field=reinterpret_cast<Field*>(0x60);
Field* role_view_field=reinterpret_cast<Field*>(0x70);
Field* controller_field=reinterpret_cast<Field*>(0x80);
Field* velocity_field=reinterpret_cast<Field*>(0x90);
Field* cross_loaded_field=reinterpret_cast<Field*>(0xA0);
Field* shot_loaded_field=reinterpret_cast<Field*>(0xB0);
Field* shot_manager_field=reinterpret_cast<Field*>(0xC0);
Field* camera_field=reinterpret_cast<Field*>(0xD0);
Field* route_field=reinterpret_cast<Field*>(0xE0);
Field* target_field=reinterpret_cast<Field*>(0xF0);
Field* manager_field=reinterpret_cast<Field*>(0x100);
Field* nav_field=reinterpret_cast<Field*>(0x110);

Object *clone_route=nullptr,*clone_target=nullptr,*clone_manager=nullptr;
int path_requests=0;bool target_present=true;
const char16_t vote_nickname[]=u"SameName";
float fall_time=0,landing_time=0;int landing_calls=0,fatal_calls=0,hurt_calls=0;
int scene=7,forwards=0,moves=0,vm_calls=0,writes=0;bool owned=true;
gameplay::Scene scene_box{7};
bool valid_scene=true,loaded_scene=true,object_alive=true,hierarchy=true,script_enabled=false,has_cached_view=true;
float scalar=2.5f;
String* forwarded_name=nullptr;
int hit_forwards=0,state_forwards=0,ground_created=0;
int plan_forwards=0,camera_forwards=0,motion_forwards=0,fall_forwards=0,fire_forwards=0;
int gas_events=0,gas_rpcs=0;bool last_chest=false;gameplay::MessageInfo last_info{};
int grab_forwards=0,trigger_forwards=0;
int controller_forwards=0,clone_owner=2,leader_number=3;bool local_master=false;float nav_speed=3;
int position_forwards=0;gameplay::Vec3 last_position{};
bool clone_script_enabled=true;
gameplay::Vec3 forwarded_motion{};
float forwarded_delay=0,delta=.25f;
bool cross_loaded_value=false,shot_loaded_value=false,fire_loaded=false,consume_shot=false;
int info_size=24;uint32_t info_alignment=8;
uint8_t hand_item=0,last_entity=0,last_state=0;bool last_sync=false;
std::vector<uint8_t> dropped;
gameplay::Vec3 position{2,3,4},scale_value{.8f,.8f,.8f};gameplay::Quat rotation{0,0,0,1};
gameplay::Vec3 body_position{4,5,6},velocity{3,-100,2};
const Method* M(uintptr_t n){return reinterpret_cast<const Method*>(n);}
Handle NewRoot(Object* o,bool){roots[++next_root]=o;return next_root;}
Object* GetRoot(Handle h){return roots.at(h);}
void FreeRoot(Handle h){assert(roots.erase(h)==1);}
void ReadField(Object* o,Field* f,void* out){
 if(f==scalar_field&&o==fall_object){*static_cast<float*>(out)=fall_time;return;}
 if(f==view_field){assert(o==pawn);*static_cast<Object**>(out)=view;}
 else if(f==iterator_field){assert(o==iterator);*static_cast<Object**>(out)=iterator_owner;}
 else if(f==scalar_field){assert(o==pawn);*static_cast<float*>(out)=scalar;}
 else if(f==attack_field){assert(o==attack_object);*static_cast<Object**>(out)=view;}
 else if(f==state_field){assert(o==state_object);*static_cast<Object**>(out)=view;}
 else if(f==explosion_field){assert(o==explosion_object||o==remote_explosion);*static_cast<Object**>(out)=o==explosion_object?view:clone_view;}
 else if(f==drop_field){assert(o==pawn);*static_cast<Object**>(out)=point_object;}
 else if(f==role_view_field){assert(o==script_object||o==movement_object||o==fall_object||o==cross_object||o==shot_object);*static_cast<Object**>(out)=view;}
 else if(f==controller_field){assert(o==movement_object);*static_cast<Object**>(out)=body_controller;}
 else if(f==velocity_field){assert(o==movement_object);*static_cast<gameplay::Vec3*>(out)=velocity;}
 else if(f==cross_loaded_field){assert(o==cross_object);*static_cast<bool*>(out)=cross_loaded_value;}
 else if(f==shot_loaded_field){assert(o==shot_manager_object);*static_cast<bool*>(out)=shot_loaded_value;}
 else if(f==shot_manager_field){assert(o==shot_object);*static_cast<Object**>(out)=shot_manager_object;}
 else if(f==camera_field){assert(o==movement_object);*static_cast<Object**>(out)=body_transform;}
 else if(f==route_field){assert(o==pawn||o==other);*static_cast<Object**>(out)=o==pawn?body_transform:clone_route;}
 else if(f==target_field){assert(o==other);*static_cast<Object**>(out)=clone_target;}
 else if(f==manager_field){assert(o==pawn||o==other);*static_cast<Object**>(out)=o==pawn?game_object:clone_manager;}
 else if(f==nav_field){assert(o==other);*static_cast<Object**>(out)=pawn;}
 else assert(false);
}
void WriteField(Object* o,Field* f,void* value){
 if(f==scalar_field&&o==fall_object){fall_time=*static_cast<float*>(value);return;}
 if(f==velocity_field){assert(o==movement_object);velocity=*static_cast<gameplay::Vec3*>(value);return;}
 if(f==cross_loaded_field){assert(o==cross_object);cross_loaded_value=*static_cast<bool*>(value);return;}
 if(f==shot_loaded_field){assert(o==shot_manager_object);shot_loaded_value=*static_cast<bool*>(value);return;}
 assert(o==pawn&&f==scalar_field);scalar=*static_cast<float*>(value);++writes;
}
Class* ObjectClass(Object* o){assert(o==pawn||o==other||o==movement_object||o==view);return reinterpret_cast<Class*>(0x900);}
Field* FindField(Class*,const char* name){if(!std::strcmp(name,"nav1"))return route_field;if(!std::strcmp(name,"target"))return target_field;if(!std::strcmp(name,"gameManager"))return manager_field;if(!std::strcmp(name,"navComponent"))return nav_field;if(!std::strcmp(name,"controller"))return controller_field;if(!std::strcmp(name,"dropPointTransform"))return drop_field;return has_cached_view&&(!std::strcmp(name,"qsa")||!std::strcmp(name,"pv"))?view_field:nullptr;}
const Type* FieldTypeForTest(Field* f){return reinterpret_cast<const Type*>((f==route_field||f==target_field)?0x2000:f==manager_field?0x3000:0);}
Class* ClassForType(const Type* t){return reinterpret_cast<Class*>(const_cast<Type*>(t));}
const char* ClassName(Class* c){return c==reinterpret_cast<Class*>(0x2000)?"Transform":c==reinterpret_cast<Class*>(0x3000)?"GameObject":"Fixture";}
const char* ClassNamespace(Class*){return "UnityEngine";}
void SetObjectField(Object* o,Field* f,Object* v){assert(o==other);if(f==route_field)clone_route=v;else if(f==target_field)clone_target=v;else if(f==manager_field)clone_manager=v;else assert(false);}
int32_t NickLength(String*){return 8;}
const uint16_t* NickChars(String*){return reinterpret_cast<const uint16_t*>(vote_nickname);}
String* NewString(const char*){return reinterpret_cast<String*>(0xF00);}
const Type* TypeForClass(Class* c){return reinterpret_cast<const Type*>(c);}
Object* ReflectedType(const Type* t){return reinterpret_cast<Object*>(const_cast<Type*>(t));}
Object* Invoke(const Method* method,void* self,void** args,Object** error){
 ++vm_calls;*error=nullptr;
 if(method==M(1)){scene_box.handle=scene;return reinterpret_cast<Object*>(&scene_box);}
 if(method==M(2)){assert(self==view);return reinterpret_cast<Object*>(&owned);}
 if(method==M(3))return room;
 if(method==M(4)){assert(args&&*static_cast<int*>(args[0])==scene);return reinterpret_cast<Object*>(&valid_scene);}
 if(method==M(5)){assert(args&&*static_cast<int*>(args[0])==scene);return reinterpret_cast<Object*>(&loaded_scene);}
 if(method==M(6)){assert(self==pawn);return view;}
 if(method==M(7)){assert(args&&args[0]);return reinterpret_cast<Object*>(&object_alive);}
 if(method==M(8)){assert(self==pawn||self==other);return game_object;}
 if(method==M(9)){assert(self==game_object);return reinterpret_cast<Object*>(&hierarchy);}
 if(method==M(10)){assert(self==pawn);return reinterpret_cast<Object*>(&script_enabled);}
 if(method==M(11))return local_actor;
 if(method==M(12)){assert(self==pawn);return reinterpret_cast<Object*>(&hand_item);}
 if(method==M(13)){assert(self==pawn&&args&&args[0]);if(hand_item)dropped.push_back(hand_item);hand_item=*static_cast<uint8_t*>(args[0]);return nullptr;}
 if(method==M(14)){assert(self==point_object);return reinterpret_cast<Object*>(&position);}
 if(method==M(15)){assert(self==point_object);return reinterpret_cast<Object*>(&rotation);}
 if(method==M(16)||method==M(17))return reinterpret_cast<Object*>(0xF10);
 if(method==M(18)){assert(args&&args[0]&&args[1]&&args[2]&&args[3]);++ground_created;return reinterpret_cast<Object*>(0xF20);}
 if(method==M(19)){assert(self==pawn||self==other);return reinterpret_cast<Object*>(self==pawn?&script_enabled:&clone_script_enabled);}
 if(method==M(20)){assert(self==pawn||self==other);(self==pawn?script_enabled:clone_script_enabled)=*static_cast<bool*>(args[0]);return nullptr;}
 if(method==M(21)){assert(self==pawn);return reinterpret_cast<Object*>(&scale_value);}
 if(method==M(22)){assert(self==pawn);scale_value=*static_cast<gameplay::Vec3*>(args[0]);return nullptr;}
 if(method==M(23)){assert(self==body_controller);return body_transform;}
 if(method==M(24)){assert(self==body_transform);return reinterpret_cast<Object*>(&body_position);}
 if(method==M(25)){assert(self==body_transform);body_position=*static_cast<gameplay::Vec3*>(args[0]);return nullptr;}
 if(method==M(26))return reinterpret_cast<Object*>(&delta);
 if(method==M(27)){assert(self==body_transform);return reinterpret_cast<Object*>(&rotation);}
 if(method==M(28)){assert(self==pawn);return reinterpret_cast<Object*>(&nav_speed);}
 if(method==M(29)){assert(self==pawn);nav_speed=*static_cast<float*>(args[0]);return nullptr;}
 if(method==M(33)){assert(self==view||self==clone_view);return reinterpret_cast<Object*>(&clone_owner);}
 if(method==M(34))return local_actor;
 if(method==M(35)){assert(self==local_actor);return reinterpret_cast<Object*>(&leader_number);}
 if(method==M(36))return reinterpret_cast<Object*>(&local_master);
 if(method==M(37))return game_object;
 if(method==M(38)){static int n=3;assert(self==game_object);return reinterpret_cast<Object*>(&n);}
 if(method==M(39)){assert(self==game_object);const int i=*static_cast<int*>(args[0]);assert(i>=0&&i<3);return i==0?local_actor:i==1?(target_present?other:nullptr):pawn;}
 if(method==M(40)){static int id;id=self==local_actor?1:self==other?2:self==pawn?3:0;return reinterpret_cast<Object*>(&id);}
 if(method==M(41))return reinterpret_cast<Object*>(const_cast<char16_t*>(vote_nickname));
 if(method==M(42)){assert(false);return nullptr;}
 if(method==M(43)){assert(self==pawn&&args[0]);++path_requests;return reinterpret_cast<Object*>(&owned);}
 if(method==M(44)){assert(self==body_transform);rotation=*static_cast<gameplay::Quat*>(args[0]);return nullptr;}
 if(method==M(45)){assert(self==game_object&&args);return child_array;}
 if(method==M(46)){static int count=1;assert(self==child_array);return reinterpret_cast<Object*>(&count);}
 if(method==M(47)){assert(self==child_array&&*static_cast<int*>(args[0])==0);return other;}
 if(method==M(48)){assert(self==game_object);return body_transform;}
 if(method==M(49)){assert(self==game_object||self==other);return self==game_object?body_transform:point_object;}
 if(method==M(50)){static bool below=false;return reinterpret_cast<Object*>(&below);}
 if(method==M(51)){assert(self==body_controller||self==other);return self==body_controller?game_object:other;}
 assert(false);return nullptr;
}
void* Unbox(Object* o){return o;}
void Original(Object*,const Method*){++forwards;}
void OriginalGas(Object*,Object* sender,Object* args,const Method* method){assert(sender==other&&args==game_object&&method==M(99));++gas_events;}
void OriginalGasRpc(Object*,bool chest,gameplay::MessageInfo info,const Method* method){assert(method==M(99));++gas_rpcs;last_chest=chest;last_info=info;}
void OriginalGrab(Object*,Object*,const Method*){++grab_forwards;}
void OriginalTrigger(Object*,Object*,const Method*){++trigger_forwards;}
void OriginalNamed(Object*,String* name,const Method*){++forwards;forwarded_name=name;}
bool OriginalMove(Object*,const Method*){++moves;return true;}
void OriginalHit(Object*,Object*,uint8_t entity,const Method*){++hit_forwards;last_entity=entity;}
void OriginalState(Object*,uint8_t value,bool sync,const Method*){++state_forwards;last_state=value;last_sync=sync;}
void OriginalSync(Object*,uint8_t value,const Method*){++state_forwards;last_state=value;}
void OriginalPlan(Object*,Object*,uint8_t entity,float delay,const Method*){++plan_forwards;last_entity=entity;forwarded_delay=delay;}
void OriginalCamera(Object*,uint8_t entity,const Method*){++camera_forwards;last_entity=entity;}
void OriginalMotion(Object*,const Method*){++motion_forwards;}
void OriginalFall(Object*,const Method*){++fall_forwards;}
void NativeLanding(Object*,const Method*){++landing_calls;landing_time=fall_time;}
void NativeFatal(Object*,const Method*){++fatal_calls;}
void NativeHurt(Object*,const Method*){++hurt_calls;}
void OriginalCrossFire(Object*,const Method*){++fire_forwards;fire_loaded=cross_loaded_value;if(consume_shot)cross_loaded_value=false;}
void OriginalShotFire(Object*,const Method*){++fire_forwards;fire_loaded=shot_loaded_value;if(consume_shot)shot_loaded_value=false;}
int OriginalControllerMove(Object* object,gameplay::Vec3 motion,const Method*){assert(object==body_controller);++controller_forwards;forwarded_motion=motion;return 5;}
void OriginalPosition(Object*,gameplay::Vec3 p,const Method*){++position_forwards;last_position=p;}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
extern "C" int DobbyHook(void*,void*,void**){return -1;}
extern "C" int DobbyHookNear(void*,void*,void**){return -1;}
int main(){
 {
  Api abi{};
  abi.method_get_param_count=[](const Method*)->uint32_t{return 2;};
  abi.method_get_param=[](const Method*,uint32_t i){return reinterpret_cast<const Type*>(uintptr_t(i+1));};
  abi.class_from_type=[](const Type* t){return reinterpret_cast<Class*>(const_cast<Type*>(t));};
  abi.class_get_name=[](Class* c){return reinterpret_cast<uintptr_t>(c)==1?"Boolean":"PhotonMessageInfo";};
  abi.class_get_namespace=[](Class* c){return reinterpret_cast<uintptr_t>(c)==1?"System":"Photon.Pun";};
  abi.class_value_size=[](Class*,uint32_t* a)->int32_t{*a=info_alignment;return info_size;};
  assert(gameplay::ExplosionRpcAbi(abi,M(99)));
  info_size=16;assert(!gameplay::ExplosionRpcAbi(abi,M(99)));info_size=24;
  info_alignment=4;assert(!gameplay::ExplosionRpcAbi(abi,M(99)));info_alignment=8;
  abi.class_get_name=[](Class*){return "WrongType";};assert(!gameplay::ExplosionRpcAbi(abi,M(99)));
  abi.method_get_param=nullptr;assert(!gameplay::ExplosionRpcAbi(abi,M(99)));
  assert(!gameplay::ExplosionRpcAbi(abi,nullptr));
 }
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
  gameplay::protected_scene=scene;gameplay::original_legacy_explosion=Original;gameplay::original_legacy_burning=Original;
  const int legacy_before=forwards;gameplay::protect=true;
  gameplay::LegacyExplosion(pawn,nullptr);gameplay::LegacyBurning(pawn,nullptr);assert(forwards==legacy_before);
  gameplay::LegacyExplosion(other,nullptr);assert(forwards==legacy_before+1);
  gameplay::protect=false;gameplay::LegacyExplosion(pawn,nullptr);gameplay::LegacyBurning(pawn,nullptr);assert(forwards==legacy_before+3);
 }
 assert(roots.empty());
 {
  gameplay::Root attack(api,attack_object),state(api,state_object),script(api,script_object);
  gameplay::protected_attack=attack.h;gameplay::protected_state=state.h;
  gameplay::attack_view=attack_field;gameplay::state_view=state_field;gameplay::local_player_method=M(11);
  gameplay::protected_scene=scene;gameplay::protected_room=reinterpret_cast<uintptr_t>(room);
  gameplay::original_bat=OriginalHit;gameplay::original_eat=OriginalHit;gameplay::original_bite=OriginalHit;
  gameplay::original_bat_rpc=OriginalHit;gameplay::original_eat_rpc=OriginalHit;gameplay::original_bite_rpc=OriginalHit;
  gameplay::original_state_change=OriginalState;gameplay::original_state_sync=OriginalSync;
  gameplay::original_lava=Original;
  gameplay::original_plan_death=OriginalPlan;gameplay::original_death_camera=OriginalCamera;
  gameplay::protected_script=script.h;gameplay::script_view=role_view_field;
  gameplay::protect=false;const int before=vm_calls;
  gameplay::Bat(attack_object,local_actor,2,nullptr);assert(hit_forwards==1&&last_entity==2&&vm_calls==before);
  gameplay::protect=true;
  gameplay::Bat(attack_object,local_actor,0,nullptr);gameplay::EatRpc(attack_object,local_actor,2,nullptr);
  assert(hit_forwards==1);
  gameplay::BiteRpc(attack_object,other,3,nullptr);assert(hit_forwards==2&&last_entity==3);
  gameplay::BatRpc(other,local_actor,1,nullptr);assert(hit_forwards==2); // another receiver still targets our protected player
  gameplay::PlanDeathForPlayer(room,local_actor,0,8.5f,nullptr);assert(plan_forwards==0);
  gameplay::PlanDeathForPlayer(room,other,2,8.5f,nullptr);assert(plan_forwards==1&&last_entity==2&&forwarded_delay==8.5f);
  gameplay::DeathCamera(script_object,0,nullptr);assert(camera_forwards==0);
  gameplay::DeathCamera(other,3,nullptr);assert(camera_forwards==1&&last_entity==3);
  gameplay::ChangeState(state_object,1,true,nullptr);gameplay::SyncState(state_object,1,nullptr);assert(state_forwards==0);
  gameplay::ChangeState(state_object,2,false,nullptr);assert(state_forwards==1&&last_state==2&&!last_sync);
  // Gas protection blocks only this victim, before outgoing death RPC/events.
  gameplay::explosion_view=explosion_field;gameplay::original_gas_event=OriginalGas;gameplay::original_explosion_rpc=OriginalGasRpc;
  gameplay::MessageInfo info{123,0,other,clone_view};
  gameplay::GasExplosionEvent(explosion_object,other,game_object,M(99));
  gameplay::PlayerExplosionRpc(explosion_object,false,info,M(99));assert(gas_events==0&&gas_rpcs==0);
  gameplay::GasExplosionEvent(remote_explosion,other,game_object,M(99));
  gameplay::PlayerExplosionRpc(remote_explosion,false,info,M(99));assert(gas_events==1&&gas_rpcs==1&&last_info.time==123&&last_info.sender==other&&last_info.view==clone_view&&!last_chest);
  gameplay::PlayerExplosionRpc(explosion_object,true,info,M(99));assert(gas_rpcs==2&&last_chest); // chest flow unchanged
  gameplay::protect=false;gameplay::GasExplosionEvent(explosion_object,other,game_object,M(99));
  gameplay::PlayerExplosionRpc(explosion_object,false,info,M(99));assert(gas_events==2&&gas_rpcs==3);gameplay::protect=true;
  owned=false;gameplay::GasExplosionEvent(explosion_object,other,game_object,M(99));assert(gas_events==3);owned=true;
  ++scene;gameplay::PlayerExplosionRpc(explosion_object,false,info,M(99));assert(gas_rpcs==4);--scene;
  auto* saved_room=room;room=other;gameplay::GasExplosionEvent(explosion_object,other,game_object,M(99));assert(gas_events==4);room=saved_room;
  std::thread worker([&]{gameplay::GasExplosionEvent(explosion_object,other,game_object,M(99));});worker.join();assert(gas_events==5);
  gameplay::explosion_view=nullptr;gameplay::PlayerExplosionRpc(explosion_object,false,info,M(99));assert(gas_rpcs==5);
  owned=false;gameplay::ChangeState(state_object,1,true,nullptr);assert(state_forwards==2&&last_state==1&&last_sync);owned=true;
  ++scene;gameplay::Eat(attack_object,local_actor,2,nullptr);assert(hit_forwards==3);--scene;
  const int before_lava=forwards;gameplay::Lava(attack_object,nullptr);assert(forwards==before_lava);
  {
   gameplay::Runtime r(api);gameplay::runtime=&r;r.go=M(51);r.snap.ready=true;
   gameplay::Root movement(api,movement_object);gameplay::tool_movement=movement.h;
   gameplay::movement_view=role_view_field;gameplay::movement_controller=controller_field;
   gameplay::tool_transform=M(23);gameplay::tool_go_transform=M(49);gameplay::tool_is_child=M(50);
   gameplay::original_mom_grab=OriginalGrab;gameplay::original_enemy_trigger[0]=OriginalTrigger;
   gameplay::MomGrab(pawn,game_object,nullptr);assert(grab_forwards==0);
   gameplay::MomGrab(pawn,other,nullptr);assert(grab_forwards==1);
   gameplay::PausedEnemyTrigger<0>(pawn,body_controller,nullptr);assert(trigger_forwards==0);
   gameplay::PausedEnemyTrigger<0>(pawn,other,nullptr);assert(trigger_forwards==1);
   gameplay::protect=false;gameplay::MomGrab(pawn,game_object,nullptr);assert(grab_forwards==2);
   gameplay::PausedEnemyTrigger<0>(pawn,body_controller,nullptr);assert(trigger_forwards==2);gameplay::protect=true;
   gameplay::runtime=nullptr;gameplay::tool_movement=0;
  }
  gameplay::protect=false;gameplay::Lava(attack_object,nullptr);assert(forwards==before_lava+1);
  gameplay::protected_attack=gameplay::protected_state=gameplay::protected_script=0;
 }
 assert(roots.empty());
 {
  gameplay::Root move(api,movement_object),fall(api,fall_object),cross(api,cross_object),shot(api,shot_object),inventory(api,pawn);
  gameplay::tool_movement=move.h;gameplay::tool_fall=fall.h;gameplay::tool_crossbow=cross.h;gameplay::tool_shotgun=shot.h;gameplay::tool_inventory=inventory.h;
  gameplay::movement_view=gameplay::fall_view=gameplay::crossbow_view=gameplay::shotgun_view=role_view_field;
  gameplay::movement_controller=controller_field;gameplay::movement_velocity=velocity_field;gameplay::inventory_view=view_field;
  gameplay::movement_camera=camera_field;gameplay::tool_rotation=M(27);
  gameplay::crossbow_loaded=cross_loaded_field;gameplay::shotgun_loaded=shot_loaded_field;gameplay::shotgun_manager=shot_manager_field;
  gameplay::tool_transform=M(23);gameplay::tool_position=M(24);gameplay::tool_set_position=M(25);gameplay::tool_delta=M(26);gameplay::tool_hand=M(12);
  gameplay::original_motion=OriginalMotion;gameplay::original_fall_update=OriginalFall;gameplay::original_cross_fire=OriginalCrossFire;gameplay::original_shot_fire=OriginalShotFire;
  gameplay::original_controller_move=OriginalControllerMove;
  const int before=vm_calls;gameplay::ghost_enabled=false;gameplay::Motion(movement_object,nullptr);assert(motion_forwards==1&&vm_calls==before);
  gameplay::ghost_enabled=true;gameplay::Motion(movement_object,nullptr);
  assert(motion_forwards==1&&gameplay::Near(body_position,gameplay::Vec3{4,5,6})&&velocity.y==0);
  assert(gameplay::GhostControllerMove(body_controller,{.75f,0,.5f},nullptr)==0);
  assert(controller_forwards==0&&gameplay::Near(body_position,gameplay::Vec3{4.75f,5,6.5f}));
  for(int i=0;i<10;++i)assert(gameplay::GhostControllerMove(body_controller,{0,-100,0},nullptr)==0&&body_position.y==5);
  gameplay::FallUpdate(fall_object,nullptr);assert(fall_forwards==0);
  owned=false;gameplay::Motion(movement_object,nullptr);gameplay::FallUpdate(fall_object,nullptr);assert(motion_forwards==2&&fall_forwards==1);
  assert(gameplay::GhostControllerMove(body_controller,{1,2,3},nullptr)==5&&controller_forwards==1&&gameplay::Near(forwarded_motion,{1,2,3}));owned=true;
  gameplay::ghost_enabled=false;gameplay::FallUpdate(fall_object,nullptr);assert(fall_forwards==2);
  assert(gameplay::GhostControllerMove(body_controller,{3,2,1},nullptr)==5&&controller_forwards==2);
  gameplay::original_fall_landing=NativeLanding;gameplay::original_fall_fatal=NativeFatal;gameplay::original_fall_hurt=NativeHurt;gameplay::fall_elapsed=scalar_field;
  gameplay::protect=true;fall_time=9;gameplay::FallLanding(fall_object,nullptr);gameplay::FallFatal(fall_object,nullptr);gameplay::FallHurt(fall_object,nullptr);
  assert(landing_calls==1&&landing_time==0&&fatal_calls==0&&hurt_calls==0); // landing state still executes
  gameplay::FallFatal(other,nullptr);gameplay::FallHurt(other,nullptr);assert(fatal_calls==1&&hurt_calls==1);
  gameplay::protect=false;fall_time=9;gameplay::FallLanding(fall_object,nullptr);gameplay::FallFatal(fall_object,nullptr);gameplay::FallHurt(fall_object,nullptr);
  assert(landing_calls==2&&landing_time==9&&fatal_calls==2&&hurt_calls==2);
  gameplay::protect=true;owned=false;fall_time=7;gameplay::FallLanding(fall_object,nullptr);gameplay::FallFatal(fall_object,nullptr);assert(landing_time==7&&fatal_calls==3);owned=true;
  const auto fall_thread=gameplay::unity_thread;gameplay::unity_thread={};fall_time=8;gameplay::FallLanding(fall_object,nullptr);assert(landing_time==8);gameplay::unity_thread=fall_thread;
  ++scene;fall_time=6;gameplay::FallLanding(fall_object,nullptr);assert(landing_time==6);--scene;
  hand_item=9;gameplay::ammo_enabled=false;gameplay::CrossFire(cross_object,nullptr);assert(!fire_loaded&&fire_forwards==1);
  gameplay::protect=true;gameplay::ammo_enabled=true;gameplay::CrossFire(cross_object,nullptr);assert(fire_loaded&&!cross_loaded_value&&fire_forwards==2);
  consume_shot=true;gameplay::CrossFire(cross_object,nullptr);assert(fire_loaded&&!cross_loaded_value);consume_shot=false;
  hand_item=25;gameplay::ShotFire(shot_object,nullptr);assert(fire_loaded&&!shot_loaded_value);
  owned=false;gameplay::ShotFire(shot_object,nullptr);assert(!fire_loaded);owned=true;
  hand_item=1;gameplay::ShotFire(shot_object,nullptr);assert(!fire_loaded);
  hand_item=25;gameplay::protect=false;gameplay::ShotFire(shot_object,nullptr);assert(!fire_loaded); // stale ammo flag cannot outlive God
  gameplay::ammo_enabled=false;gameplay::tool_movement=gameplay::tool_fall=gameplay::tool_crossbow=gameplay::tool_shotgun=gameplay::tool_inventory=0;hand_item=0;
 }
 assert(roots.empty());
 {
  gameplay::Runtime r(api);r.snap.ready=r.snap.god_ready=r.snap.infinity_ready=true;
  r.SetGod(true);assert(r.snap.god&&r.snap.infinity);
  r.SetGod(false);assert(!r.snap.god&&!r.snap.infinity);
  r.snap.infinity_ready=false;r.SetGod(true);assert(r.snap.god&&!r.snap.infinity);
  r.snap.infinity_ready=true;gameplay_bridge::SyncGodAmmo(r.snap);assert(r.snap.infinity);
  r.snap.god_ready=false;r.SetGod(true);assert(!r.snap.god&&!r.snap.infinity);
  r.snap.god_ready=true;r.SetGod(true);r.snap.ready=false;gameplay_bridge::SyncGodAmmo(r.snap);assert(!r.snap.infinity);
  r.Clear(false);assert(!r.snap.god&&!r.snap.infinity&&!gameplay::ammo_enabled);
 }
 {
  // Match the actual stripped player: isStopped, isOnNavMesh and Warp do not
  // exist. A successful animation freeze must also stop autonomous navigation.
  gameplay::Runtime runtime(api);runtime.alive=M(7);runtime.enabled_get=M(19);runtime.enabled_set=M(20);runtime.nav_get=M(28);runtime.nav_set=M(29);
  runtime.pos_get=runtime.local_pos_get=M(24);runtime.rot_get=runtime.local_rot_get=M(27);runtime.pos_set=M(25);runtime.rot_set=M(44);
  auto actor=std::make_unique<gameplay::Actor>(api,pawn,0);actor->nav.Set(pawn);actor->transform.Set(body_transform);runtime.actors.push_back(std::move(actor));
  gameplay::protected_scene=scene;gameplay::protected_room=reinterpret_cast<uintptr_t>(room);
  runtime.snap.ready=true;gameplay::runtime=&runtime;runtime.settings[0].paused=true;script_enabled=true;
  const auto anchor=body_position;
  runtime.Apply(*runtime.actors[0]);assert(nav_speed==0&&script_enabled&&gameplay::EnemyPaused(pawn,0)&&!gameplay::EnemyPaused(other,0));
  for(int i=0;i<8;++i){nav_speed=7;body_position={99,99,99};runtime.Apply(*runtime.actors[0]);assert(nav_speed==0&&gameplay::Near(body_position,anchor));}
  gameplay::original_position=gameplay::original_local_position=OriginalPosition;
  gameplay::EnemyPosition(body_transform,{9,9,9},nullptr);assert(gameplay::Near(last_position,anchor));
  gameplay::EnemyLocalPosition(body_transform,{9,9,9},nullptr);assert(gameplay::Near(last_position,anchor));
  gameplay::EnemyPosition(other,{9,9,9},nullptr);assert(gameplay::Near(last_position,{9,9,9}));
  const int calls=vm_calls;std::thread off_thread([]{gameplay::EnemyPosition(body_transform,{8,8,8},nullptr);});off_thread.join();assert(vm_calls==calls&&gameplay::Near(last_position,{8,8,8}));
  gameplay::original_enemy_update[0]=OriginalMotion;const int tick_before=motion_forwards;
  gameplay::EnemyUpdate<0>(pawn,nullptr);assert(motion_forwards==tick_before);
  runtime.settings[0].paused=false;runtime.Apply(*runtime.actors[0]);assert(nav_speed==7&&script_enabled&&!gameplay::EnemyPaused(pawn,0));
  gameplay::EnemyPosition(body_transform,{7,7,7},nullptr);assert(gameplay::Near(last_position,{7,7,7}));
  gameplay::EnemyUpdate<0>(pawn,nullptr);assert(motion_forwards==tick_before+1);
  motion_forwards=0;
  runtime.settings[0].paused=true;runtime.Apply(*runtime.actors[0]);script_enabled=false;
  runtime.settings[0].paused=false;runtime.Apply(*runtime.actors[0]);assert(runtime.actors[0]->nav_changed&&nav_speed==0);
  script_enabled=true;runtime.Apply(*runtime.actors[0]);assert(nav_speed==7&&!runtime.actors[0]->nav_changed);
  gameplay::runtime=nullptr;
 }
 assert(roots.empty());
 {
  gameplay::Runtime runtime(api);runtime.alive=M(7);runtime.master_client=M(34);runtime.actor_number=M(35);runtime.owner_number=M(33);runtime.master=M(36);runtime.mine=M(2);
  runtime.clones.push_back(std::make_unique<gameplay::CloneRecord>(api,game_object,view,gameplay::Vec3{},gameplay_bridge::Now()-1));
  clone_owner=2;runtime.settings[0].paused=true;runtime.UpdateClones();
  assert(!runtime.clones.front()->finished&&runtime.clones.front()->attempts==0);
  runtime.settings[0].paused=false;runtime.clones.front()->born=gameplay_bridge::Now()-1;
  for(int attempt=0;attempt<3;++attempt){runtime.clones.front()->next=0;runtime.UpdateClones();}
  assert(runtime.clones.front()->finished&&runtime.snap.status.find(u8"devriye başlatılmadı")!=std::string::npos);
  runtime.clones.clear();clone_owner=leader_number;
  runtime.clones.push_back(std::make_unique<gameplay::CloneRecord>(api,game_object,view,gameplay::Vec3{},gameplay_bridge::Now()-1));
  runtime.UpdateClones();assert(runtime.clones.front()->finished&&runtime.snap.status.find(u8"eşleşti")!=std::string::npos&&runtime.snap.status.find(u8"doğrulanamaz")!=std::string::npos);
 }
 assert(roots.empty());
 {
  gameplay::Runtime runtime(api);runtime.movement.Set(movement_object);runtime.alive=M(7);runtime.transform=M(23);runtime.pos_get=M(24);runtime.rot_get=M(27);runtime.pos_set=M(25);
  runtime.snap.ghost_ready=runtime.snap.ghost=runtime.ghost_active=true;runtime.ghost_origin={4,5,6};
  body_position.y=20;
  assert(runtime.ReturnGhost()&&gameplay::Near(body_position,runtime.ghost_origin)&&!runtime.snap.ghost);
 }
 assert(roots.empty());
 {
  gameplay::Vec3 p{};gameplay::Quat q{};
  assert(gameplay::FrontPose({10,2,20},{0,0,0,1},2,p,q)&&gameplay::Near(p,{10,2,22})&&q.x==0&&q.z==0);
  assert(gameplay::FrontPose({10,2,20},{0,.70710678f,0,.70710678f},2,p,q)&&gameplay::Near(p,{12,2,20}));
  assert(gameplay::FrontPose({10,2,20},{.5f,0,0,.8660254f},2,p,q)&&gameplay::Near(p,{10,2,22})&&q.x==0&&q.z==0);
  assert(!gameplay::FrontPose({0,0,0},{0,0,0,0},2,p,q));
  assert(std::string(gameplay_catalog::Name(9))=="Crossbow"&&std::string(gameplay_catalog::Name(25))=="Shotgun"&&std::string(gameplay_catalog::Name(48))=="Shotgun Ammo");
 }
 {
  gameplay::Runtime runtime(api);gameplay::Actor actor(api,pawn,0);
  // This case explicitly models a disabled AI script on an active owned GO;
  // it must not inherit the enabled state from the pause/resume case above.
  script_enabled=false;
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
 {
  gameplay::Vec3 delta{};const gameplay::Quat level{0,0,0,1};
  assert(gameplay::FlightMotion({0,-100,1},level,level,delta)&&gameplay::Near(delta,{0,0,1}));
  assert(gameplay::FlightMotion({0,-100,1},{-.70710678f,0,0,.70710678f},level,delta)&&gameplay::Near(delta,{0,1,0}));
  assert(gameplay::FlightMotion({0,-100,1},{.70710678f,0,0,.70710678f},level,delta)&&gameplay::Near(delta,{0,-1,0}));
  assert(gameplay::FlightMotion({1,-100,0},{-.70710678f,0,0,.70710678f},level,delta)&&gameplay::Near(delta,{1,0,0}));
  assert(gameplay::FlightMotion({0,-100,0},level,level,delta)&&gameplay::Near(delta,{0,0,0}));
 }
 {
  api.field_get_type=FieldTypeForTest;api.class_from_type=ClassForType;api.class_get_name=ClassName;api.class_get_namespace=ClassNamespace;api.field_set_value_object=SetObjectField;
  gameplay::Runtime r(api);r.alive=M(7);r.go=M(8);r.go_active=M(9);r.pos_get=M(24);r.rot_get=M(27);r.enabled_get=M(19);r.nav_destination=M(43);
  script_enabled=true;
  r.actors.push_back(std::make_unique<gameplay::Actor>(api,pawn,0));
  assert(r.BindCloneRoute(other,0)&&clone_route==body_transform&&clone_target==body_transform&&clone_manager==game_object&&path_requests==1);
  assert(r.Ref(pawn,"nav1")==body_transform&&r.Ref(pawn,"gameManager")==game_object); // original scene bindings untouched
  api.class_get_type=TypeForClass;api.type_get_object=ReflectedType;
  r.master_client=M(34);r.actor_number=M(35);r.owner_number=M(33);r.master=M(36);r.enabled_set=M(20);
  r.classes[0]=reinterpret_cast<Class*>(0x5000);r.children=M(45);r.length=M(46);r.item=M(47);r.go_transform=M(48);r.pos_set=M(25);
  local_master=true;clone_owner=leader_number;const double born=Now()-1;
  r.clones.push_back(std::make_unique<gameplay::CloneRecord>(api,game_object,clone_view,body_position,born));
  r.clones[0]->behavior_ready=true; // behavior bindings have their own full fixture
  r.settings[0].paused=true;r.UpdateClones();assert(!r.clones[0]->route_initialized&&r.clones[0]->attempts==0); // pause doesn't consume startup retries
  r.clones[0]->born=born;r.clones[0]->next=0;r.settings[0].paused=false;
  r.UpdateClones();assert(r.clones[0]->route_initialized&&r.clones[0]->placement_ready&&!r.clones[0]->finished&&path_requests==2);
  r.clones[0]->next=0;r.UpdateClones();assert(!r.clones[0]->finished); // accepted path alone isn't motion
  body_position.x+=.2f;r.clones[0]->next=0;r.UpdateClones();assert(!r.clones[0]->finished); // autonomous nav movement is not a completed AI frame
  gameplay::runtime=&r;gameplay::CompletedEnemyFrame(other,0);gameplay::runtime=nullptr;
  r.clones[0]->next=0;r.UpdateClones();assert(r.clones[0]->finished&&r.snap.status.find(u8"özgün AI karesinin")!=std::string::npos);
  api.class_get_type=nullptr;api.type_get_object=nullptr;
  api.field_get_type=nullptr;api.class_from_type=nullptr;api.class_get_name=nullptr;api.class_get_namespace=nullptr;api.field_set_value_object=nullptr;
 }
 assert(roots.empty());
 {
  api.string_length=NickLength;api.string_chars=NickChars;
  gameplay::Runtime r(api);r.snap.ready=r.snap.authority=true;r.room_id=reinterpret_cast<uintptr_t>(room);r.current_room=M(3);r.master=M(36);r.actor_number=M(40);r.vote_local=M(11);
  r.vote_players=M(37);r.length=M(38);r.item=M(39);r.vote_nick=M(41);
  local_master=true;const double now=Now();
  assert(r.ballot.Start(1,2,"SameName",10,{{1,"Host"},{2,"SameName"},{3,"SameName"}},now));r.ballot.Open(now,1000);
  r.IncomingVote(other,"SameName: /hayır",1001);r.IncomingVote(pawn,"SameName: /evet",1001);
  assert(r.ballot.yes==2&&r.ballot.no==1);
  r.IncomingVote(other,"SameName: /evet",1002);r.IncomingVote(pawn,"SameName: /hayır",1002);assert(r.ballot.yes==2&&r.ballot.no==1);
  r.IncomingVote(game_object,"SameName: /evet",1002);assert(r.ballot.yes==2); // unknown sender, despite matching display name
  local_master=false;r.snap.authority=false;assert(r.VoteRoomReady()); // client may initiate/count
  r.EndVote("cancelled");assert(!r.ballot.Active()&&!gameplay::vote_listening);
  gameplay::vote_receive_ready=false;gameplay::vote_decode=nullptr;
  api.string_length=nullptr;api.string_chars=nullptr;api.field_static_get_value=nullptr;api.field_static_set_value=nullptr;
 }
 assert(roots.empty());
 Clear();Snapshot s;s.ready=true;s.context=10;Publish(s);
 Command speed_command{Kind::PlayerSpeed,10};speed_command.factor=std::numeric_limits<float>::quiet_NaN();assert(!Queue(speed_command));
 speed_command.factor=10;assert(Queue(speed_command));speed_command.factor=2;assert(Queue(speed_command));auto speed_queue=Take();assert(speed_queue.size()==1&&speed_queue.front().factor==2);
 Command old{Kind::Spawn,9};assert(!Queue(old));
 Command tune{Kind::EnemySettings,10};tune.settings.speed=std::numeric_limits<float>::quiet_NaN();assert(!Queue(tune));
 tune.settings.speed=20;assert(Queue(tune));tune.settings.speed=2;assert(Queue(tune));
 auto q=Take();assert(q.size()==1&&q.front().settings.speed==2);
 tune.enemy=4;tune.settings.size=50;assert(Queue(tune));q=Take();assert(q.front().settings.size==20);
 s.context=11;Publish(s);assert(!Queue(tune));
 Command removed{Kind::CloneEnemy,11};removed.enemy=3;assert(!Queue(removed));removed.enemy=5;assert(!Queue(removed));
 Command clone{Kind::CloneEnemy,11};clone.settings.speed=2;clone.settings.size=1.5f;clone.clone_distance=4;
 assert(Queue(clone));q=Take();assert(q.size()==1&&q[0].settings.speed==2&&q[0].settings.size==1.5f&&q[0].clone_distance==4);
 clone.clone_distance=std::numeric_limits<float>::infinity();assert(!Queue(clone));
 Command spawn{Kind::Spawn,11};for(int i=0;i<24;++i)assert(Queue(spawn));assert(!Queue(spawn));Clear();
 std::cout<<"Gameplay: own-target God/death-day/camera guards, scoped ghost flight, stripped-API navigation pause/resume and position-write guards, clone route binding, fall damage own-player/scene/thread guards, authenticated client prank votes, owned infinite ammo, item flow and GC roots: PASS\n";
}
