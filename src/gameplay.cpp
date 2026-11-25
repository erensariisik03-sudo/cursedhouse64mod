#include "gameplay.h"
#include "gameplay_bridge.h"
#include "gameplay_catalog.h"
#include "chat_bot_core.h"
#include "chat_send_gate.h"
#include "dobby.h"
#include <android/log.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <thread>
#include <utility>
namespace gameplay {
namespace {
using namespace il2cpp;
using gameplay_bridge::Enemy;
constexpr size_t kEnemyCount=gameplay_bridge::kEnemies;
struct Vec3{float x,y,z;};struct Quat{float x,y,z,w;};struct Scene{int handle;};
struct Root {
    const Api& api;Handle h=0;
    Root(const Api& a,Object* p=nullptr):api(a){Set(p);}
    ~Root(){if(h)api.gchandle_free(h);}
    Root(const Root&)=delete;
    void Set(Object* p){Handle n=p?api.gchandle_new(p,false):0;if(h)api.gchandle_free(h);h=n;}
    Object* Get()const{return h?api.gchandle_get_target(h):nullptr;}
};
const Api* hook_api=nullptr;
std::thread::id unity_thread;
bool hooks_installed=false,god_hooks_ready=false,protect=false;
Handle protected_player=0,protected_manager=0;
int protected_scene=0;
const Method* scene_method=nullptr;
const Method *mine_method=nullptr,*room_method=nullptr,*view_method=nullptr,*scene_valid_method=nullptr,*scene_loaded_method=nullptr;
Field* player_view=nullptr;
uintptr_t protected_room=0;
using Unary=void(*)(Object*,const Method*);
using Named=void(*)(Object*,String*,const Method*);
using Move=bool(*)(Object*,const Method*);
Unary original_die=nullptr,original_ai=nullptr,original_spider=nullptr,original_spider_ai=nullptr;
Named original_named=nullptr;
Move original_mom=nullptr;
Field* mom_owner=nullptr;
bool modern_hooks_ready=false;
Handle protected_attack=0,protected_state=0;
Field *attack_view=nullptr,*state_view=nullptr;
Handle protected_script=0;
Field* script_view=nullptr;
const Method* local_player_method=nullptr;
using Hit=void(*)(Object*,Object*,uint8_t,const Method*);
using StateChange=void(*)(Object*,uint8_t,bool,const Method*);
using StateSync=void(*)(Object*,uint8_t,const Method*);
Hit original_bat=nullptr,original_eat=nullptr,original_bite=nullptr,original_bat_rpc=nullptr,original_eat_rpc=nullptr,original_bite_rpc=nullptr;
Unary original_lava=nullptr;
StateChange original_state_change=nullptr;StateSync original_state_sync=nullptr;
using PlanDeath=void(*)(Object*,Object*,uint8_t,float,const Method*);
using EntityAction=void(*)(Object*,uint8_t,const Method*);
PlanDeath original_plan_death=nullptr;EntityAction original_death_camera=nullptr;
bool ReadScene(const Api& api,const Method* get,const Method* valid,const Method* loaded,int& handle){
    bool ok=false;Root boxed(api,api.Call(get,nullptr,nullptr,&ok));if(!ok||!boxed.Get())return false;
    auto* value=static_cast<Scene*>(api.object_unbox(boxed.Get()));if(!value)return false;
    const int current=value->handle;int argument=current;void* args[]={&argument};
    Root is_valid(api,api.Call(valid,nullptr,args,&ok));if(!ok||!is_valid.Get()||!*static_cast<bool*>(api.object_unbox(is_valid.Get())))return false;
    Root is_loaded(api,api.Call(loaded,nullptr,args,&ok));if(!ok||!is_loaded.Get()||!*static_cast<bool*>(api.object_unbox(is_loaded.Get())))return false;
    // Scene handles are opaque signed identifiers, not non-negative indices.
    handle=current;return true;
}
bool Protected(Object* object,bool manager=false){
    if(std::this_thread::get_id()!=unity_thread)return false;
    if(!protect||!object||!hook_api)return false;
    const Handle h=manager?protected_manager:protected_player;
    if(!h||hook_api->gchandle_get_target(h)!=object||!protected_player)return false;
    int current=0;if(!ReadScene(*hook_api,scene_method,scene_valid_method,scene_loaded_method,current)||current!=protected_scene)return false;
    if(reinterpret_cast<uintptr_t>(hook_api->Call(room_method))!=protected_room||!protected_room)return false;
    auto* player=hook_api->gchandle_get_target(protected_player);if(!player)return false;
    Object* view=nullptr;if(player_view)hook_api->field_get_value(player,player_view,&view);
    if(!view&&view_method)view=hook_api->Call(view_method,player);
    Root pv(*hook_api,view);
    return pv.Get()&&hook_api->Value<bool>(mine_method,pv.Get(),false);
}
bool ModernProtected(Object* object,bool state=false){
    if(std::this_thread::get_id()!=unity_thread||!protect||!hook_api||!object)return false;
    const Handle handle=state?protected_state:protected_attack;Field* field=state?state_view:attack_view;
    if(!handle||!field||hook_api->gchandle_get_target(handle)!=object)return false;
    int current=0;if(!ReadScene(*hook_api,scene_method,scene_valid_method,scene_loaded_method,current)||current!=protected_scene)return false;
    if(!protected_room||reinterpret_cast<uintptr_t>(hook_api->Call(room_method))!=protected_room)return false;
    Object* view=nullptr;hook_api->field_get_value(object,field,&view);Root pv(*hook_api,view);
    return pv.Get()&&hook_api->Value<bool>(mine_method,pv.Get(),false);
}
bool LocalGodTarget(Object* target){
    if(std::this_thread::get_id()!=unity_thread||!protect||!hook_api||!protected_state||!target)return false;
    return ModernProtected(hook_api->gchandle_get_target(protected_state),true)&&hook_api->Call(local_player_method)==target;
}
// The victim hit RPC broadcasts death events before checking its target and
// starts a deferred fatal coroutine. Enemy attack animations run separately
// in the enemy controller; forwarding this RPC is not needed for its swing.
bool BlockHit(Object* object,Object* target){return LocalGodTarget(target)||(ModernProtected(object)&&target&&hook_api->Call(local_player_method)==target);}
#define OWN_HIT(name,original) void name(Object* o,Object* target,uint8_t entity,const Method* m){if(!BlockHit(o,target))original(o,target,entity,m);}
OWN_HIT(Bat,original_bat) OWN_HIT(Eat,original_eat) OWN_HIT(Bite,original_bite)
OWN_HIT(BatRpc,original_bat_rpc) OWN_HIT(EatRpc,original_eat_rpc) OWN_HIT(BiteRpc,original_bite_rpc)
#undef OWN_HIT
void Lava(Object* o,const Method* m){if(!ModernProtected(o))original_lava(o,m);}
void ChangeState(Object* o,uint8_t value,bool sync,const Method* m){if(value!=1||!ModernProtected(o,true))original_state_change(o,value,sync,m);}
void SyncState(Object* o,uint8_t value,const Method* m){if(value!=1||!ModernProtected(o,true))original_state_sync(o,value,m);}
void PlanDeathForPlayer(Object* o,Object* target,uint8_t entity,float delay,const Method* m){if(!LocalGodTarget(target))original_plan_death(o,target,entity,delay,m);}
void DeathCamera(Object* o,uint8_t entity,const Method* m){
    bool block=false;
    if(std::this_thread::get_id()==unity_thread&&protect&&hook_api&&protected_script&&script_view&&hook_api->gchandle_get_target(protected_script)==o){
        Object* view=nullptr;hook_api->field_get_value(o,script_view,&view);Root pv(*hook_api,view);
        block=pv.Get()&&hook_api->Value<bool>(mine_method,pv.Get(),false)&&LocalGodTarget(hook_api->Call(local_player_method));
    }
    if(!block)original_death_camera(o,entity,m);
}
void Die(Object* o,const Method* m){if(!Protected(o,true))original_die(o,m);}
void AiDeath(Object* o,const Method* m){if(!Protected(o))original_ai(o,m);}
void SpiderDeath(Object* o,const Method* m){if(!Protected(o))original_spider(o,m);}
void SpiderAiDeath(Object* o,const Method* m){if(!Protected(o))original_spider_ai(o,m);}
void NamedDeath(Object* o,String* name,const Method* m){if(!Protected(o))original_named(o,name,m);}
bool MomDeath(Object* iterator,const Method* m){
    if(std::this_thread::get_id()==unity_thread&&protect&&iterator&&hook_api&&mom_owner){
        Object* owner=nullptr;hook_api->field_get_value(iterator,mom_owner,&owner);
        Root root(*hook_api,owner);if(Protected(root.Get()))return false;
    }
    return original_mom(iterator,m);
}
template<class T> bool Hook(const Method* method,void* replacement,T& original){
    void* entry=Entry(method);if(!entry)return false;
    return DobbyHook(entry,replacement,reinterpret_cast<void**>(&original))==0&&original;
}
void InstallModern(const Api& api){
    auto* attack=api.Find("","HandleEnemyAttacked");auto* state=api.Find("","PlayerStateManager");
    if(!attack||!state)return;
    attack_view=api.class_get_field_from_name(attack,"pv");state_view=api.class_get_field_from_name(state,"pv");
    local_player_method=api.FindMethod(api.Find("Photon.Pun","PhotonNetwork"),"get_LocalPlayer",0);
    if(!attack_view||!state_view||!local_player_method)return;
    bool ready=true;
#define MODERN(c,n,arity,replacement,original) {const bool ok=Hook(api.FindMethod(c,n,arity),reinterpret_cast<void*>(replacement),original);ready=ok&&ready;}
    MODERN(attack,"hiw",2,Bat,original_bat) MODERN(attack,"hiy",2,Eat,original_eat) MODERN(attack,"hja",2,Bite,original_bite)
    MODERN(attack,"RPC_GetBatted",2,BatRpc,original_bat_rpc) MODERN(attack,"RPC_GetEatenBySpiderMom",2,EatRpc,original_eat_rpc) MODERN(attack,"RPC_GetBitBySpider",2,BiteRpc,original_bite_rpc)
    MODERN(attack,"hjb",0,Lava,original_lava)
    MODERN(state,"ibe",2,ChangeState,original_state_change) MODERN(state,"RPC_SyncCurrentPlayerState",1,SyncState,original_state_sync)
    auto* days=api.Find("","DaysManager");auto* script=api.Find("","PlayerScriptManager");
    script_view=script?api.class_get_field_from_name(script,"pv"):nullptr;
    MODERN(days,"foq",3,PlanDeathForPlayer,original_plan_death)
    MODERN(script,"iai",1,DeathCamera,original_death_camera)
#undef MODERN
    modern_hooks_ready=ready&&script_view;
}
bool ghost_enabled=false,ammo_enabled=false,ghost_hook_ready=false,ammo_hooks_ready=false;
float ghost_height=0;bool ghost_height_ready=false;
Handle tool_movement=0,tool_fall=0,tool_crossbow=0,tool_shotgun=0,tool_inventory=0;
Field *movement_view=nullptr,*movement_controller=nullptr,*movement_velocity=nullptr,*movement_camera=nullptr,*fall_view=nullptr,
      *crossbow_view=nullptr,*crossbow_loaded=nullptr,*shotgun_view=nullptr,*shotgun_manager=nullptr,*shotgun_loaded=nullptr,*inventory_view=nullptr;
const Method *tool_transform=nullptr,*tool_position=nullptr,*tool_rotation=nullptr,*tool_set_position=nullptr,*tool_delta=nullptr,*tool_hand=nullptr,*tool_go_transform=nullptr,*tool_is_child=nullptr;
Unary original_motion=nullptr,original_fall_update=nullptr,original_cross_fire=nullptr,original_shot_fire=nullptr;
Unary original_fall_landing=nullptr,original_fall_fatal=nullptr,original_fall_hurt=nullptr;
Field* fall_elapsed=nullptr;bool fall_god_ready=false;uint64_t fall_blocked=0;
using ControllerMove=int(*)(Object*,Vec3,const Method*);
ControllerMove original_controller_move=nullptr;
std::array<Unary,gameplay_bridge::kEnemies> original_enemy_update{};
std::array<bool,gameplay_bridge::kEnemies> enemy_update_ready{};
using EnemyTrigger=void(*)(Object*,Object*,const Method*);
std::array<EnemyTrigger,4> original_enemy_trigger{};
bool EnemyPaused(Object* object,size_t kind);
void GuardGodEnemy(Object* object,size_t kind);
void CompletedEnemyFrame(Object* object,size_t kind);
void ReportNoise(Object* object);
void ReportFootstep(Object* object);
Unary original_footstep=nullptr;bool footstep_hook_ready=false;
void FootstepEmitted(Object* object,const Method* method){
    if(std::this_thread::get_id()!=unity_thread||!hook_api){original_footstep(object,method);return;}
    Root source(*hook_api,object);original_footstep(object,method);ReportFootstep(source.Get());
}
using NoiseRoutine=Object*(*)(Object*,const Method*);
std::array<NoiseRoutine,2> original_noise{};std::array<bool,2> noise_hooks_ready{};
template<size_t K> Object* NoiseEmitted(Object* object,const Method* method){
    if(std::this_thread::get_id()!=unity_thread||!hook_api)return original_noise[K](object,method);
    Root source(*hook_api,object),result(*hook_api,original_noise[K](object,method));ReportNoise(source.Get());return result.Get();
}

// Scoped item assistance uses the native raycast, prerequisite and interaction
// event paths. Only the local inventory's temporary enum changes; no item is
// equipped, consumed or granted and the real held item is restored on exit.
bool FieldType(const Api& api,Field* field,const char* space,const char* name);
using SightRequest=void(*)(Object*,bool,bool,const Method*);
using SightRpc=void(*)(Object*,bool,const Method*);
SightRequest original_sight_request=nullptr;SightRpc original_sight_rpc=nullptr;
bool sight_hooks_ready=false;
bool OverrideSight(Object* object,bool requested,bool remember);
bool IgnoreOwnCollider(Object* collider,size_t kind);
void SightRequestHook(Object* o,bool on,bool sync,const Method* m){original_sight_request(o,OverrideSight(o,on,true)?false:on,sync,m);}
void SightRpcHook(Object* o,bool on,const Method* m){original_sight_rpc(o,OverrideSight(o,on,false)?false:on,m);}
void InstallSight(const Api& api){
    auto* c=api.Find("","HandleLocalSeeTargetToggling");if(!c)return;
    const auto* request=api.FindMethod(c,"hpg",2);const auto* rpc=api.FindMethod(c,"RPC_SyncToggleGrannySeeTarget",1);
    if(!request||!rpc||!api.method_get_param||!api.class_from_type||!FieldType(api,api.class_get_field_from_name(c,"pv"),"Photon.Pun","PhotonView")||!FieldType(api,api.class_get_field_from_name(c,"grannySeeTarget"),"UnityEngine","GameObject"))return;
    for(auto* method:{request,rpc})for(unsigned i=0;i<(method==request?2u:1u);++i){auto* t=api.class_from_type(api.method_get_param(method,i));if(!t||std::strcmp(api.class_get_namespace(t),"System")||std::strcmp(api.class_get_name(t),"Boolean"))return;}
    const bool a=Hook(request,reinterpret_cast<void*>(SightRequestHook),original_sight_request);
    const bool b=Hook(rpc,reinterpret_cast<void*>(SightRpcHook),original_sight_rpc);sight_hooks_ready=a&&b;
    __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG sight-hooks ready=%d",sight_hooks_ready);
}
Field *assist_hand=nullptr,*assist_target=nullptr;
bool assist_hooks_ready=false;
Unary original_interaction_ray=nullptr;
using RefGetter=Object*(*)(Object*,const Method*);
RefGetter original_requirements=nullptr;
Move original_require_placing=nullptr;
bool CanAssistInventory(Object* object);
bool AssistRequirement(Object* target,Object* requirements);
bool AssistedTarget(Object* target);
struct AssistScope {
    Root inventory,target;uint8_t before=0,last=0;bool changed=false,assisted=false;
    explicit AssistScope(Object* o):inventory(*hook_api,o),target(*hook_api){hook_api->field_get_value(o,assist_hand,&before);last=before;}
    ~AssistScope(){if(changed){uint8_t now=0;hook_api->field_get_value(inventory.Get(),assist_hand,&now);if(now==last)hook_api->field_set_value(inventory.Get(),assist_hand,&before);}}
};
thread_local AssistScope* assist_scope=nullptr;
void InteractionRay(Object* o,const Method* m){
    if(assist_scope||!CanAssistInventory(o)){original_interaction_ray(o,m);return;}
    AssistScope scope(o);
    struct Enter {AssistScope* previous;explicit Enter(AssistScope* s):previous(assist_scope){assist_scope=s;}~Enter(){assist_scope=previous;}} enter(&scope);
    original_interaction_ray(o,m);
}
Object* InteractionRequirements(Object* o,const Method* m){Object* list=original_requirements(o,m);if(assist_scope)AssistRequirement(o,list);return list;}
bool InteractionPlacing(Object* o,const Method* m){return AssistedTarget(o)?false:original_require_placing(o,m);}
std::array<std::array<Unary,3>,2> original_difficulty_helpers{};
std::array<bool,2> difficulty_helpers_ready{};
thread_local Handle difficulty_target=0;
template<size_t Kind,size_t Helper> void DifficultyHelper(Object* o,const Method* m){
    if(std::this_thread::get_id()==unity_thread&&difficulty_target&&hook_api&&hook_api->gchandle_get_target(difficulty_target)==o)return;
    original_difficulty_helpers[Kind][Helper](o,m);
}
void InstallProgression(const Api& api){
    auto* inv=api.Find("","HandlePickUpItemInteractions");auto* world=api.Find("","WorldInteractable");
    assist_hand=inv?api.class_get_field_from_name(inv,"currentObjectInHand"):nullptr;
    assist_target=inv?api.class_get_field_from_name(inv,"rgn"):nullptr;
    if(FieldType(api,assist_hand,"","PickUpObject")&&assist_target&&api.class_value_size){
        uint32_t alignment=0;auto* type=api.class_from_type(api.field_get_type(assist_hand));
        if(type&&api.class_value_size(type,&alignment)==1){
            const bool a=Hook(api.FindMethod(inv,"hqw",0),reinterpret_cast<void*>(InteractionRay),original_interaction_ray);
            const bool b=DobbyHookNear(Entry(api.FindMethod(world,"fge",0)),reinterpret_cast<void*>(InteractionRequirements),reinterpret_cast<void**>(&original_requirements))==0&&original_requirements;
            const bool c=DobbyHookNear(Entry(api.FindMethod(world,"fgj",0)),reinterpret_cast<void*>(InteractionPlacing),reinterpret_cast<void**>(&original_require_placing))==0&&original_require_placing;
            assist_hooks_ready=a&&b&&c;
        }
    }
    // The human difficulty initializer normally resets combat and chooses a
    // random route before setting speeds. Suppress only these three helpers
    // during our rooted, host-only refresh; normal starts are forwarded.
    const char* helper_names[2][3]={{"eim","eil","ein"},{"duo","dun","dup"}};
    void* helper_hooks[2][3]={{reinterpret_cast<void*>(DifficultyHelper<0,0>),reinterpret_cast<void*>(DifficultyHelper<0,1>),reinterpret_cast<void*>(DifficultyHelper<0,2>)},{reinterpret_cast<void*>(DifficultyHelper<1,0>),reinterpret_cast<void*>(DifficultyHelper<1,1>),reinterpret_cast<void*>(DifficultyHelper<1,2>)}};
    for(size_t k=0;k<2;++k){auto* c=api.Find("",k?"EnemyAIGrandpaMulti":"EnemyAIGrannyMulti");bool ready=true;
        for(size_t j=0;j<3;++j)ready=Hook(api.FindMethod(c,helper_names[k][j],0),helper_hooks[k][j],original_difficulty_helpers[k][j])&&ready;
        difficulty_helpers_ready[k]=ready;
    }
}
bool FrozenEnemyPose(Object* transform,bool local,Vec3* position,Quat* rotation);
using PositionSetter=void(*)(Object*,Vec3,const Method*);
using RotationSetter=void(*)(Object*,Quat,const Method*);
using PoseSetter=void(*)(Object*,Vec3,Quat,const Method*);
PositionSetter original_position=nullptr,original_local_position=nullptr;
RotationSetter original_rotation=nullptr,original_local_rotation=nullptr;
PoseSetter original_pose=nullptr;
void EnemyPosition(Object* o,Vec3 p,const Method* m){FrozenEnemyPose(o,false,&p,nullptr);original_position(o,p,m);}
void EnemyLocalPosition(Object* o,Vec3 p,const Method* m){FrozenEnemyPose(o,true,&p,nullptr);original_local_position(o,p,m);}
void EnemyRotation(Object* o,Quat q,const Method* m){FrozenEnemyPose(o,false,nullptr,&q);original_rotation(o,q,m);}
void EnemyLocalRotation(Object* o,Quat q,const Method* m){FrozenEnemyPose(o,true,nullptr,&q);original_local_rotation(o,q,m);}
void EnemyPose(Object* o,Vec3 p,Quat q,const Method* m){FrozenEnemyPose(o,false,&p,&q);original_pose(o,p,q,m);}
bool vote_listening=false;
void VoteMessage(Object* sender,const std::string& line,int32_t sent);
struct MessageInfo {int32_t time;uint32_t padding;Object* sender;Object* view;};
static_assert(sizeof(MessageInfo)==24&&offsetof(MessageInfo,sender)==8);
using GasEvent=void(*)(Object*,Object*,Object*,const Method*);
using ExplosionRpc=void(*)(Object*,bool,MessageInfo,const Method*);
GasEvent original_gas_event=nullptr;
ExplosionRpc original_explosion_rpc=nullptr;
Unary original_legacy_explosion=nullptr,original_legacy_burning=nullptr;
Field* explosion_view=nullptr;
bool explosion_hooks_ready=false,legacy_explosion_ready=false;
uint64_t gas_blocked=0,gas_forwarded=0;
double gas_log_next=0;
void TraceGas(const char* entry,bool blocked){
    if(std::this_thread::get_id()!=unity_thread)return;
    if(blocked)++gas_blocked;else ++gas_forwarded;
    const double now=gameplay_bridge::Now();if(now<gas_log_next)return;gas_log_next=now+1;
    __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG gas entry=%s blocked=%d total_blocked=%llu total_forwarded=%llu god=%d",entry,blocked,static_cast<unsigned long long>(gas_blocked),static_cast<unsigned long long>(gas_forwarded),protect);
}
bool OwnExplosion(Object* object){
    // Match the victim component's PhotonView to the already rooted local
    // state manager. A gas can's owner/sender is not necessarily its victim.
    if(!object||!explosion_view||!hook_api||std::this_thread::get_id()!=unity_thread||!protect)return false;
    if(!LocalGodTarget(hook_api->Call(local_player_method)))return false;
    Object *pv=nullptr,*state_pv=nullptr;
    hook_api->field_get_value(object,explosion_view,&pv);
    hook_api->field_get_value(hook_api->gchandle_get_target(protected_state),state_view,&state_pv);
    Root victim(*hook_api,pv),local(*hook_api,state_pv);
    return victim.Get()&&victim.Get()==local.Get()&&hook_api->Value<bool>(mine_method,victim.Get(),false);
}
void GasExplosionEvent(Object* object,Object* sender,Object* args,const Method* method){
    const bool block=OwnExplosion(object);TraceGas("local-event",block);
    if(!block)original_gas_event(object,sender,args,method);
}
void PlayerExplosionRpc(Object* object,bool chest,MessageInfo info,const Method* method){
    const bool block=!chest&&OwnExplosion(object);if(!chest)TraceGas("victim-rpc",block);
    if(!block)original_explosion_rpc(object,chest,info,method);
}
void LegacyExplosion(Object* object,const Method* method){const bool block=Protected(object);TraceGas("legacy-rpc",block);if(!block)original_legacy_explosion(object,method);}
void LegacyBurning(Object* object,const Method* method){if(!Protected(object))original_legacy_burning(object,method);}
bool ExplosionRpcAbi(const Api& api,const Method* method){
    if(!method||!api.method_get_param_count||!api.method_get_param||!api.class_from_type||!api.class_value_size||api.method_get_param_count(method)!=2)return false;
    auto* flag=api.class_from_type(api.method_get_param(method,0));auto* info=api.class_from_type(api.method_get_param(method,1));
    if(!flag||!info||std::strcmp(api.class_get_namespace(flag),"System")||std::strcmp(api.class_get_name(flag),"Boolean")||std::strcmp(api.class_get_namespace(info),"Photon.Pun")||std::strcmp(api.class_get_name(info),"PhotonMessageInfo"))return false;
    uint32_t alignment=0;return api.class_value_size(info,&alignment)==sizeof(MessageInfo)&&alignment==alignof(MessageInfo);
}
void InstallExplosion(const Api& api){
    auto* type=api.Find("","HandlePlayerChestExplosion");
    explosion_view=type?api.class_get_field_from_name(type,"pv"):nullptr;
    const auto* rpc=api.FindMethod(type,"RPC_SyncPlayerDeathExplosion",2);
    if(FieldType(api,explosion_view,"Photon.Pun","PhotonView")&&ExplosionRpcAbi(api,rpc)){
        const bool a=Hook(api.FindMethod(type,"hup",2),reinterpret_cast<void*>(GasExplosionEvent),original_gas_event);
        const bool b=Hook(rpc,reinterpret_cast<void*>(PlayerExplosionRpc),original_explosion_rpc);
        explosion_hooks_ready=a&&b;
    }
    auto* legacy=api.Find("","playerController");
    const bool a=Hook(api.FindMethod(legacy,"RPC_playerExplosionDeath",0),reinterpret_cast<void*>(LegacyExplosion),original_legacy_explosion);
    const bool b=Hook(api.FindMethod(legacy,"RPC_playPlayerBurningOnDeath",0),reinterpret_cast<void*>(LegacyBurning),original_legacy_burning);
    legacy_explosion_ready=a&&b;
    __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG explosion-hooks modern=%d legacy=%d",explosion_hooks_ready,legacy_explosion_ready);
}
using ReceiveMessage=void(*)(Object*,Object*,MessageInfo,const Method*);
ReceiveMessage original_receive=nullptr;bool vote_receive_ready=false;
const Method *vote_utf8=nullptr,*vote_decode=nullptr,*vote_length=nullptr;
const Method* ByteDecoder(const Api& api,Class* klass){
    if(!api.class_get_methods||!api.method_get_param_count||!api.method_get_param)return nullptr;
    for(int depth=0;klass&&depth<32;++depth,klass=api.class_get_parent(klass)){
        void* it=nullptr;const Method* found=nullptr;
        for(int n=0;n<1024;++n){const auto* m=api.class_get_methods(klass,&it);if(!m)break;
            if(std::strcmp(api.method_get_name(m),"GetString")||api.method_get_param_count(m)!=1)continue;
            auto* t=api.class_from_type(api.method_get_param(m,0));
            if(!t||std::strcmp(api.class_get_namespace(t),"System")||std::strcmp(api.class_get_name(t),"Byte[]"))continue;
            if(found)return nullptr;
            found=m;
        }if(found)return found;
    }return nullptr;
}
void ReceiveVoteMessage(Object* self,Object* bytes,MessageInfo info,const Method* method){
    if(std::this_thread::get_id()!=unity_thread||!hook_api||!vote_listening){original_receive(self,bytes,info,method);return;}
    Root payload(*hook_api,bytes),sender(*hook_api,info.sender);
    original_receive(self,bytes,info,method);
    if(!vote_listening||!payload.Get()||!sender.Get())return;
    const int size=hook_api->Value<int>(vote_length,payload.Get(),-1);if(size<1||size>8192)return;
    Root encoding(*hook_api,hook_api->Call(vote_utf8));if(!encoding.Get())return;
    if(!vote_decode)vote_decode=ByteDecoder(*hook_api,hook_api->object_get_class(encoding.Get()));
    if(!vote_decode)return;
    void* args[]={payload.Get()};bool ok=false;Root text(*hook_api,hook_api->Call(vote_decode,encoding.Get(),args,&ok));
    if(ok&&text.Get())VoteMessage(sender.Get(),hook_api->Utf8(reinterpret_cast<String*>(text.Get())),info.time);
}
void InstallVoteReceiver(const Api& api){
    vote_utf8=api.FindMethod(api.Find("System.Text","Encoding"),"get_UTF8",0);
    vote_length=api.FindMethod(api.Find("System","Array"),"get_Length",0);
    const auto* method=api.FindMethod(api.Find("","ChatManager"),"RPC_GetMessage",2);
    if(!vote_utf8||!vote_length||!method||!api.method_get_param||!api.class_from_type||!api.class_value_size)return;
    auto* info=api.class_from_type(api.method_get_param(method,1));
    if(!info||std::strcmp(api.class_get_namespace(info),"Photon.Pun")||std::strcmp(api.class_get_name(info),"PhotonMessageInfo"))return;
    uint32_t alignment=0;if(api.class_value_size(info,&alignment)!=static_cast<int32_t>(sizeof(MessageInfo))||alignment!=alignof(MessageInfo))return;
    vote_receive_ready=Hook(method,reinterpret_cast<void*>(ReceiveVoteMessage),original_receive);
}
template<size_t K> void EnemyUpdate(Object* object,const Method* method){if(!EnemyPaused(object,K)){GuardGodEnemy(object,K);original_enemy_update[K](object,method);CompletedEnemyFrame(object,K);GuardGodEnemy(object,K);}}
template<size_t K> void PausedEnemyTrigger(Object* object,Object* collider,const Method* method){if(!EnemyPaused(object,K)&&!IgnoreOwnCollider(collider,K))original_enemy_trigger[K](object,collider,method);}
using VictimAction=void(*)(Object*,Object*,const Method*);
VictimAction original_mom_grab=nullptr;
bool Finite(Vec3 v);
bool FlightMotion(Vec3 motion,Quat camera,Quat body,Vec3& delta){
    if(!Finite(motion))return false;
    const auto forward=[](Quat q,Vec3& f){
        const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;if(!std::isfinite(n)||n<.0001f)return false;
        f={2*(q.x*q.z+q.w*q.y)/n,2*(q.y*q.z-q.w*q.x)/n,1-2*(q.x*q.x+q.y*q.y)/n};return Finite(f);
    };
    Vec3 look{},yaw{};if(!forward(camera,look))return false;
    float length=std::sqrt(look.x*look.x+look.z*look.z);
    if(length>.01f)yaw={look.x/length,0,look.z/length};
    else{if(!forward(body,yaw))return false;length=std::sqrt(yaw.x*yaw.x+yaw.z*yaw.z);if(length<.01f)return false;yaw={yaw.x/length,0,yaw.z/length};}
    const float ahead=motion.x*yaw.x+motion.z*yaw.z,side=motion.x*yaw.z-motion.z*yaw.x;
    delta={look.x*ahead+yaw.z*side,look.y*ahead,look.z*ahead-yaw.x*side};return Finite(delta);
}
bool WorldMatches(){
    if(std::this_thread::get_id()!=unity_thread||!hook_api||!protected_room)return false;
    int scene=0;if(!ReadScene(*hook_api,scene_method,scene_valid_method,scene_loaded_method,scene)||scene!=protected_scene)return false;
    return reinterpret_cast<uintptr_t>(hook_api->Call(room_method))==protected_room;
}
bool ToolOwner(Object* o,Handle handle,Field* field){
    if(!o||!handle||!field||!WorldMatches()||hook_api->gchandle_get_target(handle)!=o)return false;
    Object* view=nullptr;hook_api->field_get_value(o,field,&view);Root pv(*hook_api,view);
    return pv.Get()&&hook_api->Value<bool>(mine_method,pv.Get(),false);
}
void Motion(Object* o,const Method* m){
    if(std::this_thread::get_id()==unity_thread&&ghost_enabled&&movement_velocity&&ToolOwner(o,tool_movement,movement_view)){
        Vec3 velocity{};hook_api->field_get_value(o,movement_velocity,&velocity);
        // hjs applies gravity. Horizontal joystick movement uses hju and a
        // separate CharacterController.Move; it is intercepted below.
        if(Finite(velocity)){velocity.y=0;hook_api->field_set_value(o,movement_velocity,&velocity);return;}
    }
    original_motion(o,m);
}
int GhostControllerMove(Object* object,Vec3 motion,const Method* method){
    if(std::this_thread::get_id()==unity_thread&&ghost_enabled&&Finite(motion)&&tool_movement&&hook_api){
        auto* movement=hook_api->gchandle_get_target(tool_movement);
        if(ToolOwner(movement,tool_movement,movement_view)){
            Object* body=nullptr;hook_api->field_get_value(movement,movement_controller,&body);
            if(body==object){
                Root t(*hook_api,hook_api->Call(tool_transform,object));bool ok=false;
                Root boxed(*hook_api,hook_api->Call(tool_position,t.Get(),nullptr,&ok));
                if(ok&&boxed.Get()){
                    Object* raw_camera=nullptr;hook_api->field_get_value(movement,movement_camera,&raw_camera);Root camera(*hook_api,raw_camera);
                    Root camera_q(*hook_api,hook_api->Call(tool_rotation,camera.Get(),nullptr,&ok));if(!ok||!camera_q.Get())return original_controller_move(object,motion,method);
                    Root body_q(*hook_api,hook_api->Call(tool_rotation,t.Get(),nullptr,&ok));if(!ok||!body_q.Get())return original_controller_move(object,motion,method);
                    Vec3 delta{},p=*static_cast<Vec3*>(hook_api->object_unbox(boxed.Get()));
                    if(FlightMotion(motion,*static_cast<Quat*>(hook_api->object_unbox(camera_q.Get())),*static_cast<Quat*>(hook_api->object_unbox(body_q.Get())),delta)){
                        p.x+=delta.x;p.z+=delta.z;p.y=(ghost_height_ready?ghost_height:p.y)+delta.y;
                        if(Finite(p)){void* args[]={&p};hook_api->Call(tool_set_position,t.Get(),args,&ok);if(ok){ghost_height=p.y;ghost_height_ready=true;return 0;}}
                    }
                }
            }
        }
    }
    return original_controller_move(object,motion,method);
}
void FallUpdate(Object* o,const Method* m){if(std::this_thread::get_id()!=unity_thread||!ghost_enabled||!ToolOwner(o,tool_fall,fall_view))original_fall_update(o,m);}
bool OwnFallProtected(Object* o){return std::this_thread::get_id()==unity_thread&&protect&&ToolOwner(o,tool_fall,fall_view);}
void FallLanding(Object* o,const Method* m){
    // Preserve grounded/landing bookkeeping. Only the own God player's verified
    // native damage evaluator sees zero accumulated fall time.
    if(OwnFallProtected(o)&&fall_elapsed){float elapsed=0;hook_api->field_get_value(o,fall_elapsed,&elapsed);if(elapsed>0)++fall_blocked;float zero=0;hook_api->field_set_value(o,fall_elapsed,&zero);}
    original_fall_landing(o,m);
}
void FallFatal(Object* o,const Method* m){if(!OwnFallProtected(o))original_fall_fatal(o,m);}
void FallHurt(Object* o,const Method* m){if(!OwnFallProtected(o))original_fall_hurt(o,m);}
bool FieldType(const Api& api,Field* field,const char* ns,const char* name){
    if(!field||!api.field_get_type||!api.class_from_type)return false;
    auto* c=api.class_from_type(api.field_get_type(field));return c&&!std::strcmp(api.class_get_namespace(c),ns)&&!std::strcmp(api.class_get_name(c),name);
}
void InfiniteFire(Object* o,const Method* m,bool cross){
    const auto original=cross?original_cross_fire:original_shot_fire;
    if(std::this_thread::get_id()!=unity_thread||!protect||!ammo_enabled||!ToolOwner(o,cross?tool_crossbow:tool_shotgun,cross?crossbow_view:shotgun_view)||!tool_inventory){original(o,m);return;}
    auto* inv=hook_api->gchandle_get_target(tool_inventory);
    if(!ToolOwner(inv,tool_inventory,inventory_view)||hook_api->Value<uint8_t>(tool_hand,inv,0)!=(cross?9:25)){original(o,m);return;}
    Object* manager=o;if(!cross)hook_api->field_get_value(o,shotgun_manager,&manager);
    Root target(*hook_api,manager);Field* loaded=cross?crossbow_loaded:shotgun_loaded;
    if(!target.Get()||!loaded){original(o,m);return;}
    bool previous=false;hook_api->field_get_value(target.Get(),loaded,&previous);bool yes=true;hook_api->field_set_value(target.Get(),loaded,&yes);
    struct RestoreFlag{const Api& api;Root& target;Field* field;bool value;~RestoreFlag(){bool now=false;api.field_get_value(target.Get(),field,&now);if(now)api.field_set_value(target.Get(),field,&value);}}restore{*hook_api,target,loaded,previous};
    original(o,m);
}
void CrossFire(Object* o,const Method* m){InfiniteFire(o,m,true);}
void ShotFire(Object* o,const Method* m){InfiniteFire(o,m,false);}
bool LocalVictim(Object* target){
    if(std::this_thread::get_id()!=unity_thread||!protect||!hook_api||!target||!tool_movement||!movement_controller||!tool_go_transform||!tool_is_child)return false;
    if(!LocalGodTarget(hook_api->Call(local_player_method)))return false;
    auto* movement=hook_api->gchandle_get_target(tool_movement);if(!ToolOwner(movement,tool_movement,movement_view))return false;
    Object* value=nullptr;hook_api->field_get_value(movement,movement_controller,&value);Root controller(*hook_api,value),body(*hook_api,hook_api->Call(tool_transform,controller.Get())),t(*hook_api,hook_api->Call(tool_go_transform,target));
    if(!body.Get()||!t.Get())return false;
    if(body.Get()==t.Get())return true;
    void* args[]={body.Get()};bool ok=false;Root match(*hook_api,hook_api->Call(tool_is_child,t.Get(),args,&ok));return ok&&match.Get()&&*static_cast<bool*>(hook_api->object_unbox(match.Get()));
}
void MomGrab(Object* o,Object* target,const Method* m){if(!LocalVictim(target))original_mom_grab(o,target,m);}
void InstallTools(const Api& api){
    auto* movement=api.Find("","FirstPersonController");auto* fall=api.Find("","HandleFallDemage");auto* cross=api.Find("","HandleCrossBow");auto* shot=api.Find("","HandleShotgun");auto* manager=api.Find("","ShotgunManager");auto* inv=api.Find("","HandlePickUpItemInteractions");
    const auto field=[&](Class* c,const char* n){return c?api.class_get_field_from_name(c,n):nullptr;};
    movement_view=field(movement,"pv");movement_controller=field(movement,"controller");movement_velocity=field(movement,"rdp");movement_camera=field(movement,"cameraTrans");fall_view=field(fall,"pv");
    crossbow_view=field(cross,"pv");crossbow_loaded=field(cross,"haveSyringe");shotgun_view=field(shot,"pv");shotgun_manager=field(shot,"shotgunManager");shotgun_loaded=field(manager,"shotgunLoaded");inventory_view=field(inv,"pv");
    fall_elapsed=field(fall,"rfe");
    if(fall_view&&FieldType(api,fall_elapsed,"System","Single")){
        const bool a=Hook(api.FindMethod(fall,"hnl",0),reinterpret_cast<void*>(FallLanding),original_fall_landing);
        const bool b=Hook(api.FindMethod(fall,"hno",0),reinterpret_cast<void*>(FallFatal),original_fall_fatal);
        const bool c=Hook(api.FindMethod(fall,"hnm",0),reinterpret_cast<void*>(FallHurt),original_fall_hurt);fall_god_ready=a&&b&&c;
    }
    const auto method=[&](const char* ns,const char* c,const char* n,int count=0){return api.FindMethod(api.Find(ns,c),n,count);};
    tool_transform=method("UnityEngine","Component","get_transform");tool_position=method("UnityEngine","Transform","get_position");tool_set_position=method("UnityEngine","Transform","set_position",1);tool_delta=method("UnityEngine","Time","get_deltaTime");tool_hand=api.FindMethod(inv,"hrl",0);
    tool_rotation=method("UnityEngine","Transform","get_rotation");
    tool_go_transform=method("UnityEngine","GameObject","get_transform");tool_is_child=method("UnityEngine","Transform","IsChildOf",1);
    const bool mom_guard=Hook(api.FindMethod(api.Find("","SpiderMomAIController"),"inh",1),reinterpret_cast<void*>(MomGrab),original_mom_grab);
    modern_hooks_ready=modern_hooks_ready&&mom_guard;
    if(movement_view&&movement_controller&&fall_view&&tool_transform&&tool_position&&tool_rotation&&tool_set_position&&tool_delta&&FieldType(api,movement_velocity,"UnityEngine","Vector3")&&FieldType(api,movement_camera,"UnityEngine","Transform")){
        const bool a=Hook(api.FindMethod(movement,"hjs",0),reinterpret_cast<void*>(Motion),original_motion);
        const bool b=Hook(api.FindMethod(fall,"Update",0),reinterpret_cast<void*>(FallUpdate),original_fall_update);
        const bool c=Hook(method("UnityEngine","CharacterController","Move",1),reinterpret_cast<void*>(GhostControllerMove),original_controller_move);
        ghost_hook_ready=a&&b&&c;
    }
    if(!FieldType(api,crossbow_loaded,"System","Boolean"))crossbow_loaded=nullptr;
    if(!FieldType(api,shotgun_loaded,"System","Boolean"))shotgun_loaded=nullptr;
    if(tool_hand&&crossbow_view&&shotgun_view&&shotgun_manager&&inventory_view&&crossbow_loaded&&shotgun_loaded){
        const bool a=Hook(api.FindMethod(cross,"icd",0),reinterpret_cast<void*>(CrossFire),original_cross_fire);
        const bool b=Hook(api.FindMethod(shot,"idj",0),reinterpret_cast<void*>(ShotFire),original_shot_fire);ammo_hooks_ready=a&&b;
    }
}
struct Spec {
    const char* klass;const char* pv;const char* nav;const char* animator;
    std::array<const char*,3> speed,animation;
};
constexpr Spec specs[kEnemyCount]={
    {"EnemyAIGrannyMulti","pv","navComponent","grannyAIAnim",{"grannysVarSpeed",nullptr,nullptr},{"grannysAnimSpeed",nullptr,nullptr}},
    {"EnemyAIGrandpaMulti","pv","navComponent","grandpaAIAnim",{"grannysVarSpeed",nullptr,nullptr},{"grannysAnimSpeed",nullptr,nullptr}},
    {"SpiderMomAIController","pv","agent","momAnimator",{"wanderSpeed","chaseSpeed","fastSpeed"},{"wanderingAnimSpeed","chaseAnimSpeed","fastAnimSpeed"}},
    {"SpiderControllerMulti","pv",nullptr,"anim",{"spiderSpeed",nullptr,nullptr},{nullptr,nullptr,nullptr}},
    {"de",nullptr,nullptr,"CrowAnimHolder",{nullptr,nullptr,nullptr},{nullptr,nullptr,nullptr}},
    {"pj",nullptr,"qgy","ratAnim",{nullptr,nullptr,nullptr},{nullptr,nullptr,nullptr}}
};
void InstallEnemyUpdates(const Api& api){
    auto* t=api.Find("UnityEngine","Transform");
    Hook(api.FindMethod(t,"set_position",1),reinterpret_cast<void*>(EnemyPosition),original_position);
    Hook(api.FindMethod(t,"set_localPosition",1),reinterpret_cast<void*>(EnemyLocalPosition),original_local_position);
    Hook(api.FindMethod(t,"set_rotation",1),reinterpret_cast<void*>(EnemyRotation),original_rotation);
    Hook(api.FindMethod(t,"set_localRotation",1),reinterpret_cast<void*>(EnemyLocalRotation),original_local_rotation);
    Hook(api.FindMethod(t,"SetPositionAndRotation",2),reinterpret_cast<void*>(EnemyPose),original_pose);
    void* replacements[]={reinterpret_cast<void*>(EnemyUpdate<0>),reinterpret_cast<void*>(EnemyUpdate<1>),reinterpret_cast<void*>(EnemyUpdate<2>),reinterpret_cast<void*>(EnemyUpdate<3>),reinterpret_cast<void*>(EnemyUpdate<4>),reinterpret_cast<void*>(EnemyUpdate<5>)};
    void* triggers[]={reinterpret_cast<void*>(PausedEnemyTrigger<0>),reinterpret_cast<void*>(PausedEnemyTrigger<1>),reinterpret_cast<void*>(PausedEnemyTrigger<2>),reinterpret_cast<void*>(PausedEnemyTrigger<3>)};
    for(size_t k=0;k<kEnemyCount;++k){
        const auto* method=api.FindMethod(api.Find("",specs[k].klass),k<3?"FixedUpdate":"Update",0);
        enemy_update_ready[k]=Hook(method,replacements[k],original_enemy_update[k]);
        if(k<4){const bool ready=Hook(api.FindMethod(api.Find("",specs[k].klass),k==3?"OnTriggerEnter":"OnTriggerStay",1),triggers[k],original_enemy_trigger[k]);enemy_update_ready[k]=enemy_update_ready[k]&&ready;}
    }
}
bool Near(float a,float b){return std::fabs(a-b)<.0001f;}
bool Near(Vec3 a,Vec3 b){return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z);}
bool Finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool FrontPose(Vec3 body,Quat camera,float distance,Vec3& position,Quat& facing){
    if(!Finite(body)||!std::isfinite(distance)||distance<=0||!std::isfinite(camera.x)||!std::isfinite(camera.y)||!std::isfinite(camera.z)||!std::isfinite(camera.w))return false;
    const float norm=camera.x*camera.x+camera.y*camera.y+camera.z*camera.z+camera.w*camera.w;
    if(norm<.0001f)return false;
    const float fx=2*(camera.x*camera.z+camera.w*camera.y)/norm,fz=1-2*(camera.x*camera.x+camera.y*camera.y)/norm;
    const float length=std::sqrt(fx*fx+fz*fz);if(length<.01f)return false;
    const float x=fx/length,z=fz/length,yaw=std::atan2(-x,-z);
    position={body.x+x*distance,body.y,body.z+z*distance};facing={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};return true;
}
struct NavHit {Vec3 position{},normal{};float distance=0;int mask=0,hit=0;};
static_assert(sizeof(NavHit)==36);
struct Scalar { Field* field=nullptr;float base=0,last=0;bool changed=false; };
struct AnimationNode {Root object;bool frozen=false,enabled=true;AnimationNode(const Api& a,Object* o):object(a,o){}};
struct ScaleNode {Root object;Vec3 before{},last{};bool changed=false;ScaleNode(const Api& a,Object* o):object(a,o){}};
struct Actor {
    Root object,nav,animator,transform;size_t kind;
    std::vector<Scalar> speed_fields,animation_fields;
    float nav_base=0,nav_last=0,anim_base=1,anim_last=1;
    bool nav_changed=false,anim_changed=false;
    double attack_release=0;
    bool scale_changed=false,pose_frozen=false,local_pose_frozen=false;
    Vec3 frozen_position{},frozen_local_position{};Quat frozen_rotation{},frozen_local_rotation{};
    Vec3 scale_before{},scale_last{};
    std::vector<std::unique_ptr<AnimationNode>> animations;
    std::vector<std::unique_ptr<ScaleNode>> visuals;
    Actor(const Api& a,Object* p,size_t k):object(a,p),nav(a),animator(a),transform(a),kind(k){}
};
struct Prefab {int id;Root object;std::string name;Prefab(const Api& a,int i,Object* o,std::string n):id(i),object(a,o),name(std::move(n)){} };
struct CloneRecord {
    Root object,view,controller;Vec3 position;double born,next=0;unsigned attempts=0,completed_frames=0;bool finished=false,route_initialized=false,placement_ready=false,behavior_ready=false;
    Vec3 observed_position{};bool observation_ready=false;double probe_next=0;unsigned probes=0;
    const char* sense_issue="new";double health_next=0,still_since=0,attack_since=0;Vec3 health_position{};bool health_observed=false;unsigned recoveries=0;
    double last_seen=0,door_next=0,investigate_until=0,arrived_at=0;unsigned heard=0;bool investigating=false;Vec3 noise_position{};
    Enemy settings{};size_t kind=0,waypoint=0;double sense_next=0,players_next=0,bind_next=0;unsigned senses=0,sense_errors=0,bind_tries=0;bool has_target=false;bool custom_settings=false,client_patrol=false,client_routes_prepared=false,client_nav_enabled=false;double route_next=0;
    CloneRecord(const Api& api,Object* o,Object* pv,Vec3 p,double now):object(api,o),view(api,pv),controller(api),position(p),born(now){}
    Object* Get()const{return object.Get();}
};
struct DoorCooldown {Root object;double next;DoorCooldown(const Api& a,Object* o,double n):object(a,o),next(n){}};
struct Runtime {
    const Api& api;
    bool initialized=false,valid=false,busy=false;
    int scene=-1;uintptr_t room_id=0,player_id=0,modern_id=0,inventory_id=0;uint64_t generation=0;
    double next_scan=0;
    Root player,manager,inventory,attacked,player_state,script,movement,fall_damage,crossbow,shotgun,sight,sight_target,speed_owner;
    std::vector<Scalar> player_speeds;
    bool sight_applied=false,sight_normal=true,sight_internal=false,sight_restore_pending=false;
    double sight_next=0;
    unsigned sight_sends=0,target_releases=0;
    std::array<Class*,kEnemyCount> classes{};
    std::array<Enemy,kEnemyCount> settings{};
    std::vector<std::unique_ptr<Actor>> actors;
    std::vector<std::unique_ptr<Prefab>> prefabs;
    std::vector<std::unique_ptr<CloneRecord>> clones;
    std::vector<std::unique_ptr<DoorCooldown>> doors;
    std::vector<std::unique_ptr<Root>> noise_markers;
    Class* door_class{};std::array<Class*,2> noise_classes{};
    const Method *door_open{},*door_line{},*hit_collider{};size_t hit_size=0;
    float noise_radius=20;double footsteps_next=0;double noise_next=0,noise_time=0;unsigned noise_serial=0;Vec3 noise_position{};bool noise_baseline=false;
    const Method *nav_update_pos{},*nav_update_rot{};
    Class *nav_class{};
    bool ghost_active=false,cross_visual=false,shot_visual=false,cross_button_saved=false,shot_button_saved=false;
    Vec3 ghost_origin{};
    gameplay_bridge::Snapshot snap;
    double diagnostic_next=0;uint64_t diagnostic_context=0;unsigned diagnostic_flags=~0u;
    void Diagnostics(){
        const unsigned flags=unsigned(snap.ready)|(unsigned(snap.authority)<<1)|(unsigned(snap.god_ready)<<2)|(unsigned(snap.god)<<3)|(unsigned(snap.infinity_ready)<<4)|(unsigned(snap.infinity)<<5)|(unsigned(snap.explosion_ready)<<6)|(unsigned(snap.invisible)<<7);
        const double now=gameplay_bridge::Now();
        if(now<diagnostic_next&&flags==diagnostic_flags&&snap.context==diagnostic_context)return;
        diagnostic_next=now+5;diagnostic_flags=flags;diagnostic_context=snap.context;
        __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG state context=%llu scene=%d ready=%d host=%d god_ready=%d god=%d gas_ready=%d ammo_ready=%d ammo=%d actors=%zu clones=%zu gas_blocked=%llu gas_forwarded=%llu",static_cast<unsigned long long>(snap.context),scene,snap.ready,snap.authority,snap.god_ready,snap.god,snap.explosion_ready,snap.infinity_ready,snap.infinity,actors.size(),clones.size(),static_cast<unsigned long long>(gas_blocked),static_cast<unsigned long long>(gas_forwarded));
        __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG ai invisible_ready=%d invisible=%d sight_sends=%u target_releases=%u fall_ready=%d fall_blocked=%llu",snap.invisible_ready,snap.invisible,sight_sends,target_releases,snap.fall_ready,static_cast<unsigned long long>(fall_blocked));
    }
    void SetGod(bool on){
        if(snap.god!=on)sight_next=0;
        snap.god=on&&snap.god_ready;gameplay_bridge::SyncGodAmmo(snap);
        snap.status=snap.god?u8"God Mode açık.":u8"God Mode kapalı.";
    }
    room_vote::Session ballot;double vote_scan=0,vote_notice_next=0;int vote_sent_yes=-1,vote_sent_no=-1;
    std::string vote_result_notice;double vote_result_deadline=0,vote_result_next=0;
    std::string player_issue=u8"Maç karakteri bekleniyor.",inventory_issue=u8"Maç envanteri bekleniyor.";
    const Method *find{},*length{},*item{},*alive{},*active{},*enabled_get{},*enabled_set{},*go{},*go_active{},
        *transform{},*scale_get{},*scale_set{},*nav_get{},*nav_set{},*local_pos_get{},*local_rot_get{},
        *anim_get{},*anim_set{},*current_room{},*master{},*queue{},*get_scene{},*mine{},*name_get{},
        *pos_get{},*rot_get{},*spawn{},*prefix_get{},*path_combine{},*network_destroy{},*local_destroy{},*view_get{},*scene_valid{},*scene_loaded{},
        *children{},*go_transform{},*is_child{},*hand_get{},*hand_pick{},*resources_load{},*rpc{},*view_id{},*find_all{},*nav_sample{},*pos_set{},*rot_set{},*active_set{},*active_get{},*cross_images{},*cross_arrow{},
        *room_spawn{},*master_client{},*actor_number{},*owner_number{},*transfer_owner{},*nav_destination{},*nav_has_path{},*nav_pending{},
        *vote_players{},*vote_nick{},*vote_local{},*vote_time{},*vote_send{},*vote_text{},*vote_set{},*vote_focused{},
        *parent_get{},*child_count{},*child_get{},*get_component{},*anim_controller_get{},*clip_length{},*nav_velocity{},*anim_bool{},*anim_float{};
    Field *vote_input_field{},*vote_allowed_field{};
    Class* vote_chat_class{};
    Field* difficulty_field=nullptr;
    std::array<const Method*,3> difficulty_refresh{};
    Class *player_class{},*manager_class{},*inventory_class{},*tie_class{},*attack_class{},*state_class{},*animator_class{},*animation_class{},*view_class{},*shotgun_class{},*pun_class{},
        *script_class{},*movement_class{},*fall_class{},*crossbow_class{},*weapon_class{},*pickup_class{},*sight_class{};
    std::array<Class*,3> clone_managers{};
    explicit Runtime(const Api& a):api(a),player(a),manager(a),inventory(a),attacked(a),player_state(a),script(a),movement(a),fall_damage(a),crossbow(a),shotgun(a),sight(a),sight_target(a),speed_owner(a){}
    const Method* ResolveMethod(const char* ns,const char* c,const char* n,int count=0){return api.FindMethod(api.Find(ns,c),n,count);}
    Object* Ref(Object* o,const char* field){
        if(!o||!field)return nullptr;
        auto* f=api.class_get_field_from_name(api.object_get_class(o),field);
        Object* p=nullptr;if(f)api.field_get_value(o,f,&p);return p;
    }
    bool Alive(Object* o){if(!o)return false;void* args[]={o};bool ok=false;auto* b=api.Call(alive,nullptr,args,&ok);return ok&&b&&*static_cast<bool*>(api.object_unbox(b));}
    bool Active(Object* o){return Alive(o)&&api.Value<bool>(active,o,false);}
    bool InHierarchy(Object* o){if(!Alive(o))return false;Root object_go(api,api.Call(go,o));return Alive(object_go.Get())&&api.Value<bool>(go_active,object_go.Get(),false);}
    bool Owned(Object* o,const char* field){
        if(!o)return false;
        Root pv(api,Ref(o,field));
        if(!pv.Get()&&view_get&&pun_class){
            auto* c=api.object_get_class(o);
            for(int n=0;c&&n<64;++n){if(c==pun_class){pv.Set(api.Call(view_get,o));break;}c=api.class_get_parent?api.class_get_parent(c):nullptr;}
        }
        return Alive(pv.Get())&&api.Value<bool>(mine,pv.Get(),false);
    }
    template<class T> bool Set(const Method* m,Object* o,T value){if(!m||!o)return false;void* args[]={&value};bool ok=false;api.Call(m,o,args,&ok);return ok;}
    template<class F> bool Each(Class* c,F&& fn){
        if(!c)return true;
        const auto* type=api.class_get_type(c);Root reflection(api,type?api.type_get_object(type):nullptr);
        if(!reflection.Get())return false;
        // Inspect disabled scripts too; callers require an active GameObject.
        int inactive=1,sort=0;void* args[]={reflection.Get(),&inactive,&sort};bool ok=false;
        Root array(api,api.Call(find,nullptr,args,&ok));if(!ok||!array.Get())return false;
        Root count(api,api.Call(length,array.Get(),nullptr,&ok));if(!ok||!count.Get())return false;
        const int n=*static_cast<int*>(api.object_unbox(count.Get()));if(n<0||n>512)return false;
        for(int i=0;i<n;++i){void* at[]={&i};Root o(api,api.Call(item,array.Get(),at,&ok));if(!ok)return false;if(Alive(o.Get()))fn(o.Get());}
        return true;
    }
    template<class F> bool Children(Object* object,Class* type,F&& fn){
        if(!object||!type||!children)return false;
        Root reflected(api,api.type_get_object(api.class_get_type(type)));if(!reflected.Get())return false;
        bool use_array=false,recursive=true,inactive=true,reverse=false;void* args[]={reflected.Get(),&use_array,&recursive,&inactive,&reverse,nullptr};bool ok=false;
        Root array(api,api.Call(children,object,args,&ok));if(!ok||!array.Get())return false;
        const int n=api.Value<int>(length,array.Get(),-1);if(n<0||n>256)return false;
        for(int i=0;i<n;++i){void* at[]={&i};Root value(api,api.Call(item,array.Get(),at,&ok));if(!ok)return false;if(Alive(value.Get()))fn(value.Get());}
        return true;
    }
    template<class F> bool AllOfType(Class* type,F&& fn){
        if(!find_all||!type)return false;
        Root reflected(api,api.type_get_object(api.class_get_type(type)));if(!reflected.Get())return false;
        void* args[]={reflected.Get()};bool ok=false;Root array(api,api.Call(find_all,nullptr,args,&ok));if(!ok||!array.Get())return false;
        const int n=api.Value<int>(length,array.Get(),-1);if(n<0||n>2048)return false;
        for(int i=0;i<n;++i){void* at[]={&i};Root value(api,api.Call(item,array.Get(),at,&ok));if(!ok)return false;if(Alive(value.Get()))fn(value.Get());}
        return true;
    }
    const Method* ExactRpc(){
        if(!api.class_get_methods||!api.method_get_param_count||!api.method_get_param)return nullptr;
        auto* c=api.Find("Photon.Pun","PhotonView");if(!c)return nullptr;
        void* it=nullptr;const Method* found=nullptr;
        for(int n=0;n<1024;++n){auto* m=api.class_get_methods(c,&it);if(!m)break;
            if(std::strcmp(api.method_get_name(m),"RPC")||api.method_get_param_count(m)!=3)continue;
            auto* t=api.class_from_type(api.method_get_param(m,1));if(!t||std::strcmp(api.class_get_name(t),"RpcTarget")||std::strcmp(api.class_get_namespace(t),"Photon.Pun"))continue;
            if(found)return nullptr;
            found=m;
        }return found;
    }
    const Method* ExactAnimator(const char* name,const char* value_type){
        if(!api.class_get_methods||!api.method_get_param_count||!api.method_get_param)return nullptr;
        auto* c=api.Find("UnityEngine","Animator");void* it=nullptr;const Method* result=nullptr;
        for(int n=0;c&&n<256;++n){auto* m=api.class_get_methods(c,&it);if(!m)break;
            if(std::strcmp(api.method_get_name(m),name)||api.method_get_param_count(m)!=2)continue;
            auto* id=api.class_from_type(api.method_get_param(m,0));auto* value=api.class_from_type(api.method_get_param(m,1));
            if(!id||!value||std::strcmp(api.class_get_namespace(id),"System")||std::strcmp(api.class_get_name(id),"Int32")||std::strcmp(api.class_get_namespace(value),"System")||std::strcmp(api.class_get_name(value),value_type))continue;
            if(result)return nullptr;
            result=m;
        }return result;
    }
    const Method* ExactComponent(){
        if(!api.class_get_methods||!api.method_get_param_count||!api.method_get_param||!api.method_is_generic)return nullptr;
        auto* c=api.Find("UnityEngine","Component");void* it=nullptr;const Method* found=nullptr;
        for(int n=0;c&&n<1024;++n){auto* m=api.class_get_methods(c,&it);if(!m)break;
            if(std::strcmp(api.method_get_name(m),"GetComponent")||api.method_get_param_count(m)!=1||api.method_is_generic(m))continue;
            auto* p=api.class_from_type(api.method_get_param(m,0));
            if(!p||std::strcmp(api.class_get_namespace(p),"System")||std::strcmp(api.class_get_name(p),"Type"))continue;
            if(found)return nullptr;
            found=m;
        }return found;
    }
    const Method* ExactNavSample(){
        if(!api.class_get_methods||!api.method_get_param_count||!api.method_get_param)return nullptr;
        auto* c=api.Find("UnityEngine.AI","NavMesh");if(!c)return nullptr;
        void* it=nullptr;const Method* found=nullptr;
        for(int n=0;n<512;++n){auto* m=api.class_get_methods(c,&it);if(!m)break;
            if(std::strcmp(api.method_get_name(m),"SamplePosition")||api.method_get_param_count(m)!=4)continue;
            auto* t=api.class_from_type(api.method_get_param(m,3));if(!t||std::strcmp(api.class_get_name(t),"Int32")||std::strcmp(api.class_get_namespace(t),"System"))continue;
            if(found)return nullptr;
            found=m;
        }return found;
    }
    const Method* ExactDestroy(){
        if(!api.class_get_methods||!api.method_get_param_count||!api.method_get_param||!api.class_from_type)return nullptr;
        auto* c=api.Find("Photon.Pun","PhotonNetwork");if(!c)return nullptr;
        void* it=nullptr;const Method* result=nullptr;
        for(int n=0;n<1024;++n){
            auto* m=api.class_get_methods(c,&it);if(!m)break;
            if(std::strcmp(api.method_get_name(m),"Destroy")||api.method_get_param_count(m)!=1)continue;
            auto* p=api.class_from_type(api.method_get_param(m,0));if(!p)continue;
            if(std::strcmp(api.class_get_namespace(p),"UnityEngine")||std::strcmp(api.class_get_name(p),"GameObject"))continue;
            if(result)return nullptr;
            result=m;
        }
        return result;
    }
    const Method* ExactTransfer(){
        if(!api.class_get_methods||!api.method_get_param_count||!api.method_get_param)return nullptr;
        auto* c=api.Find("Photon.Pun","PhotonView");void* it=nullptr;const Method* found=nullptr;
        for(int n=0;c&&n<1024;++n){const auto* m=api.class_get_methods(c,&it);if(!m)break;
            if(std::strcmp(api.method_get_name(m),"TransferOwnership")||api.method_get_param_count(m)!=1)continue;
            auto* t=api.class_from_type(api.method_get_param(m,0));
            if(!t||std::strcmp(api.class_get_namespace(t),"System")||std::strcmp(api.class_get_name(t),"Int32"))continue;
            if(found)return nullptr;
            found=m;
        }return found;
    }
    bool Resolve(){
        if(!api.domain_get||!api.field_get_value||!api.field_set_value||!api.field_get_type||!api.class_from_type||
           !api.object_get_class||!api.class_get_field_from_name||!api.class_get_type||!api.type_get_object)return false;
        find=ResolveMethod("UnityEngine","Object","FindObjectsByType",3);length=ResolveMethod("System","Array","get_Length");item=ResolveMethod("System","Array","GetValueImpl",1);
        alive=ResolveMethod("UnityEngine","Object","op_Implicit",1);active=ResolveMethod("UnityEngine","Behaviour","get_isActiveAndEnabled");
        enabled_get=ResolveMethod("UnityEngine","Behaviour","get_enabled");enabled_set=ResolveMethod("UnityEngine","Behaviour","set_enabled",1);
        go=ResolveMethod("UnityEngine","Component","get_gameObject");go_active=ResolveMethod("UnityEngine","GameObject","get_activeInHierarchy");transform=ResolveMethod("UnityEngine","Component","get_transform");
        scale_get=ResolveMethod("UnityEngine","Transform","get_localScale");scale_set=ResolveMethod("UnityEngine","Transform","set_localScale",1);
        nav_get=ResolveMethod("UnityEngine.AI","NavMeshAgent","get_speed");nav_set=ResolveMethod("UnityEngine.AI","NavMeshAgent","set_speed",1);
        // This player's stripped Unity build has no isStopped/isOnNavMesh/Warp
        // metadata. Use its actual speed/enabled/path API, never gate on a
        // missing optional method returning a false fallback.
        nav_has_path=ResolveMethod("UnityEngine.AI","NavMeshAgent","get_hasPath");nav_pending=ResolveMethod("UnityEngine.AI","NavMeshAgent","get_pathPending");
        local_pos_get=ResolveMethod("UnityEngine","Transform","get_localPosition");local_rot_get=ResolveMethod("UnityEngine","Transform","get_localRotation");
        parent_get=ResolveMethod("UnityEngine","Transform","get_parent");child_count=ResolveMethod("UnityEngine","Transform","get_childCount");child_get=ResolveMethod("UnityEngine","Transform","GetChild",1);
        get_component=ExactComponent();
        door_class=api.Find("","Door");door_open=api.FindMethod(door_class,"ewl",0);
        door_line=ResolveMethod("UnityEngine","Physics","Linecast",5);hit_collider=ResolveMethod("UnityEngine","RaycastHit","get_collider");
        auto* hit_type=api.Find("UnityEngine","RaycastHit");uint32_t hit_alignment=0;
        if(hit_type&&api.class_value_size){const int bytes=api.class_value_size(hit_type,&hit_alignment);if(bytes>0&&bytes<=512&&hit_alignment<=alignof(std::max_align_t))hit_size=size_t(bytes);}
        noise_classes={api.Find("","NoiseObjectFallsMulti"),api.Find("","NoiseObjectFallsMultiGrandpa")};
        nav_class=api.Find("UnityEngine.AI","NavMeshAgent");
        nav_update_pos=ResolveMethod("UnityEngine.AI","NavMeshAgent","set_updatePosition",1);
        nav_update_rot=ResolveMethod("UnityEngine.AI","NavMeshAgent","set_updateRotation",1);
        clip_length=ResolveMethod("UnityEngine","AnimationClip","get_length");
        nav_velocity=ResolveMethod("UnityEngine.AI","NavMeshAgent","get_velocity");
        anim_bool=ExactAnimator("SetBool","Boolean");anim_float=ExactAnimator("SetFloat","Single");
        anim_controller_get=ResolveMethod("UnityEngine","Animator","get_runtimeAnimatorController");
        anim_get=ResolveMethod("UnityEngine","Animator","get_speed");anim_set=ResolveMethod("UnityEngine","Animator","set_speed",1);
        current_room=ResolveMethod("Photon.Pun","PhotonNetwork","get_CurrentRoom");master=ResolveMethod("Photon.Pun","PhotonNetwork","get_IsMasterClient");queue=ResolveMethod("Photon.Pun","PhotonNetwork","get_IsMessageQueueRunning");
        get_scene=ResolveMethod("UnityEngine.SceneManagement","SceneManager","GetActiveScene");mine=ResolveMethod("Photon.Pun","PhotonView","get_IsMine");
        scene_valid=ResolveMethod("UnityEngine.SceneManagement","Scene","IsValidInternal",1);scene_loaded=ResolveMethod("UnityEngine.SceneManagement","Scene","GetIsLoadedInternal",1);
        view_get=ResolveMethod("Photon.Pun","MonoBehaviourPun","get_photonView");
        children=ResolveMethod("UnityEngine","GameObject","GetComponentsInternal",6);go_transform=ResolveMethod("UnityEngine","GameObject","get_transform");is_child=ResolveMethod("UnityEngine","Transform","IsChildOf",1);
        hand_get=ResolveMethod("","HandlePickUpItemInteractions","hrl");hand_pick=ResolveMethod("","HandlePickUpItemInteractions","hra",2);
        resources_load=ResolveMethod("UnityEngine","Resources","Load",2);
        rpc=ExactRpc();
        view_id=ResolveMethod("Photon.Pun","PhotonView","get_ViewID");
        find_all=ResolveMethod("UnityEngine","Resources","FindObjectsOfTypeAll",1);nav_sample=ExactNavSample();
        pos_set=ResolveMethod("UnityEngine","Transform","set_position",1);rot_set=ResolveMethod("UnityEngine","Transform","set_rotation",1);active_set=ResolveMethod("UnityEngine","GameObject","SetActive",1);active_get=ResolveMethod("UnityEngine","GameObject","get_activeSelf");
        cross_images=ResolveMethod("","HandleCrossBow","ice",1);cross_arrow=ResolveMethod("","HandleCrossBow","icg",1);
        name_get=ResolveMethod("UnityEngine","Object","get_name");pos_get=ResolveMethod("UnityEngine","Transform","get_position");rot_get=ResolveMethod("UnityEngine","Transform","get_rotation");
        spawn=ResolveMethod("Photon.Pun","PhotonNetwork","Instantiate",5);path_combine=ResolveMethod("System.IO","Path","Combine",2);
        room_spawn=ResolveMethod("Photon.Pun","PhotonNetwork","InstantiateRoomObject",5);
        master_client=ResolveMethod("Photon.Pun","PhotonNetwork","get_MasterClient");actor_number=ResolveMethod("Photon.Realtime","Player","get_ActorNumber");
        owner_number=ResolveMethod("Photon.Pun","PhotonView","get_OwnerActorNr");transfer_owner=ExactTransfer();nav_destination=ResolveMethod("UnityEngine.AI","NavMeshAgent","SetDestination",1);
        vote_players=ResolveMethod("Photon.Pun","PhotonNetwork","get_PlayerList");vote_nick=ResolveMethod("Photon.Realtime","Player","get_NickName");
        vote_local=ResolveMethod("Photon.Pun","PhotonNetwork","get_LocalPlayer");vote_time=ResolveMethod("Photon.Pun","PhotonNetwork","get_ServerTimestamp");
        vote_chat_class=api.Find("","ChatManager");vote_send=api.FindMethod(vote_chat_class,"SendMessage",0);
        vote_input_field=vote_chat_class?api.class_get_field_from_name(vote_chat_class,"inputField"):nullptr;vote_allowed_field=vote_chat_class?api.class_get_field_from_name(vote_chat_class,"canSendMessage"):nullptr;
        vote_text=ResolveMethod("TMPro","TMP_InputField","get_text");vote_set=ResolveMethod("TMPro","TMP_InputField","set_text",1);vote_focused=ResolveMethod("TMPro","TMP_InputField","get_isFocused");
        prefix_get=ResolveMethod("<PrivateImplementationDetails>{B6FBF95C-D343-4B5E-9090-977A04841BC0}","a","bvx");
        network_destroy=ExactDestroy();local_destroy=ResolveMethod("UnityEngine","Object","Destroy",1);
        player_class=api.Find("","playerController");manager_class=api.Find("","playerManager");inventory_class=api.Find("","HandlePickUpItemInteractions");tie_class=api.Find("","HandItemTie");
        attack_class=api.Find("","HandleEnemyAttacked");state_class=api.Find("","PlayerStateManager");shotgun_class=api.Find("","ShotgunManager");
        animator_class=api.Find("UnityEngine","Animator");animation_class=api.Find("UnityEngine","Animation");view_class=api.Find("Photon.Pun","PhotonView");
        pun_class=api.Find("Photon.Pun","MonoBehaviourPun");
        script_class=api.Find("","PlayerScriptManager");movement_class=api.Find("","FirstPersonController");fall_class=api.Find("","HandleFallDemage");crossbow_class=api.Find("","HandleCrossBow");weapon_class=api.Find("","HandleShotgun");pickup_class=api.Find("","PickupItem");
        sight_class=api.Find("","HandleLocalSeeTargetToggling");
        clone_managers={api.Find("","GrannyManager"),api.Find("","GrandpaManager"),api.Find("","SpiderMomManager")};
        auto* rooms=api.Find("","PhotonRoomsManager");difficulty_field=rooms?api.class_get_field_from_name(rooms,"Difficulty"):nullptr;
        if(!FieldType(api,difficulty_field,"","GameDifficulty")||!api.field_static_get_value||!api.field_static_set_value||!api.class_value_size)difficulty_field=nullptr;
        if(difficulty_field){uint32_t alignment=0;auto* type=api.class_from_type(api.field_get_type(difficulty_field));if(!type||api.class_value_size(type,&alignment)!=1)difficulty_field=nullptr;}
        difficulty_refresh={ResolveMethod("","EnemyAIGrannyMulti","eik"),ResolveMethod("","EnemyAIGrandpaMulti","dum"),ResolveMethod("","SpiderMomAIController","imw")};
        for(size_t k=0;k<kEnemyCount;++k)classes[k]=api.Find("",specs[k].klass);
        const Method* required[]={find,length,item,alive,active,enabled_get,enabled_set,go,go_active,transform,scale_get,scale_set,current_room,master,queue,get_scene,scene_valid,scene_loaded,mine};
        return (player_class||attack_class)&&std::all_of(std::begin(required),std::end(required),[](auto* m){return m!=nullptr;});
    }
    bool ProgressionHost(){return snap.ready&&snap.authority&&WorldMatches()&&api.Value<bool>(master,nullptr,false);}
    bool AssistWorld(Object* target,Object* list){
        if(!assist_scope||!CanAssistInventory(assist_scope->inventory.Get())||!target||!list)return false;
        Object* active_target=nullptr;api.field_get_value(assist_scope->inventory.Get(),assist_target,&active_target);
        Root current(api,active_target),requirements(api,list);
        if(current.Get()!=target||!InHierarchy(target)||std::strcmp(api.class_get_name(api.object_get_class(target)),"WorldInteractable")||std::strcmp(api.class_get_namespace(api.object_get_class(target)),""))return false;
        auto* klass=api.object_get_class(list);const auto* count=api.FindMethod(klass,"get_Count",0);const auto* get=api.FindMethod(klass,"get_Item",1);
        const int n=api.Value<int>(count,list,-1);if(n<1||n>32||!get)return false;
        uint8_t candidate=0,current_hand=0;api.field_get_value(assist_scope->inventory.Get(),assist_hand,&current_hand);bool found=false;
        for(int i=0;i<n;++i){void* args[]={&i};bool ok=false;Root item(api,api.Call(get,list,args,&ok));if(!ok||!item.Get())return false;
            const auto value=*static_cast<uint8_t*>(api.object_unbox(item.Get()));if(i==0)candidate=value;if(value==current_hand)found=true;}
        // Empty hands accepted by the actual list need no fabricated item.
        if(!found){api.field_set_value(assist_scope->inventory.Get(),assist_hand,&candidate);uint8_t check=0;api.field_get_value(assist_scope->inventory.Get(),assist_hand,&check);if(check!=candidate)return false;assist_scope->changed=true;assist_scope->last=candidate;}
        assist_scope->target.Set(target);assist_scope->assisted=true;return true;
    }
    bool RefreshDifficulty(Actor& actor){
        if(!ProgressionHost()||actor.kind>2||!difficulty_refresh[actor.kind]||(actor.kind<2&&!difficulty_helpers_ready[actor.kind])||!InHierarchy(actor.object.Get())||!Owned(actor.object.Get(),specs[actor.kind].pv))return false;
        // Scoped helper hooks retain human combat, target and route state.
        // Mom's initializer also appends routes: run with a disposable list and
        // restore its real route list on every exit, preserving chase/targets.
        Root routes(api),temporary(api);Field* route_field=nullptr;
        if(actor.kind==2){
            if(!api.object_new||!api.field_set_value_object)return false;
            route_field=api.class_get_field_from_name(api.object_get_class(actor.object.Get()),"wayPoints");routes.Set(Ref(actor.object.Get(),"wayPoints"));
            if(!route_field||!routes.Get())return false;
            auto* type=api.object_get_class(routes.Get());const auto* ctor=api.FindMethod(type,".ctor",0);if(!ctor)return false;
            temporary.Set(api.object_new(type));if(!temporary.Get())return false;bool ok=false;api.Call(ctor,temporary.Get(),nullptr,&ok);if(!ok)return false;
        }
        struct RouteRestore{const Api& api;Object* actor;Field* field;Root& original;~RouteRestore(){if(field)api.field_set_value_object(actor,field,original.Get());}} route_restore{api,actor.object.Get(),route_field,routes};
        if(route_field)api.field_set_value_object(actor.object.Get(),route_field,temporary.Get());
        struct DifficultyScope {Handle previous;explicit DifficultyScope(Handle target):previous(difficulty_target){difficulty_target=target;}~DifficultyScope(){difficulty_target=previous;}} scope(actor.object.h);
        bool ok=false;api.Call(difficulty_refresh[actor.kind],actor.object.Get(),nullptr,&ok);if(!ok)return false;
        for(auto* fields:{&actor.speed_fields,&actor.animation_fields})for(auto& field:*fields){float value=0;api.field_get_value(actor.object.Get(),field.field,&value);if(!std::isfinite(value)||value<0)return false;field.base=value;field.changed=false;}
        float speed=PositiveField(actor.object.Get(),actor.kind==2?"wanderSpeed":"grannysVarSpeed");
        if(speed>0&&Alive(actor.nav.Get())){actor.nav_base=speed;if(Set(nav_set,actor.nav.Get(),settings[actor.kind].paused?0.f:speed)){actor.nav_last=settings[actor.kind].paused?0.f:speed;actor.nav_changed=settings[actor.kind].paused;}}
        return true;
    }
    void ChangeDifficulty(int selection){
        if(selection<0||selection>5||!difficulty_field||!snap.difficulty_ready||!ProgressionHost()){snap.status=u8"Maç zorluğunu yalnız gerçek oda sahibi değiştirebilir.";return;}
        uint8_t before=0;api.field_static_get_value(difficulty_field,&before);uint8_t value=uint8_t(selection);api.field_static_set_value(difficulty_field,&value);uint8_t check=255;api.field_static_get_value(difficulty_field,&check);
        if(check!=value){api.field_static_set_value(difficulty_field,&before);snap.status=u8"Zorluk yazımı doğrulanamadı.";return;}
        unsigned updated=0,pending=0;for(auto& actor:actors)if(actor->kind<3){if(RefreshDifficulty(*actor))++updated;else ++pending;}
        snap.difficulty=selection;snap.status=u8"Zorluk uygulandı; AI güncellenen: "+std::to_string(updated)+u8", bekleyen: "+std::to_string(pending)+u8". Sahne/eşya presetleri yeniden kurulmaz.";
    }
    void Clear(bool restore){
        ReleaseSight(true);sight.Set(nullptr);sight_target.Set(nullptr);sight_applied=false;
        vote_listening=false;ballot.Clear();vote_scan=vote_notice_next=0;vote_sent_yes=vote_sent_no=-1;
        vote_result_notice.clear();vote_result_deadline=vote_result_next=0;
        protect=false;ghost_enabled=ammo_enabled=false;protected_player=0;protected_manager=0;protected_attack=protected_state=protected_script=0;
        tool_movement=tool_fall=tool_crossbow=tool_shotgun=tool_inventory=0;
        DisableTools(restore);
        if(restore){for(auto& clone:clones)StopClientPatrol(*clone);for(auto& actor:actors){Restore(*actor);CombatGuard(*actor,false);}}
        actors.clear();prefabs.clear();clones.clear();doors.clear();noise_markers.clear();noise_baseline=false;noise_next=noise_time=footsteps_next=0;noise_radius=20;noise_serial=0;
        player.Set(nullptr);manager.Set(nullptr);inventory.Set(nullptr);attacked.Set(nullptr);player_state.Set(nullptr);
        script.Set(nullptr);movement.Set(nullptr);fall_damage.Set(nullptr);crossbow.Set(nullptr);shotgun.Set(nullptr);
        settings={};snap={};next_scan=0;gameplay_bridge::Clear();
        modern_id=inventory_id=0;
    }
    void BuildVisuals(Actor& actor){
        const auto add_animation=[&](Object* o){if(Alive(o)&&std::none_of(actor.animations.begin(),actor.animations.end(),[&](const auto& n){return n->object.Get()==o;}))actor.animations.push_back(std::make_unique<AnimationNode>(api,o));};
        const auto scan=[&](Object* object){Children(object,animator_class,add_animation);Children(object,animation_class,add_animation);};
        Root root(api,api.Call(go,actor.object.Get()));if(Alive(root.Get()))scan(root.Get());
        if(Alive(actor.animator.Get())){add_animation(actor.animator.Get());Root holder(api,api.Call(go,actor.animator.Get()));if(Alive(holder.Get()))scan(holder.Get());}
        if(actor.kind!=4)return;
        const char* visual_fields[]={"CrowFlyAnim","CrowFlyEat","CrowFlyBack","CrowEat"};
        const auto add_scale=[&](Object* t){
            if(!Alive(t))return;
            for(const auto& n:actor.visuals){if(n->object.Get()==t)return;void* args[]={n->object.Get()};bool ok=false;Root result(api,api.Call(is_child,t,args,&ok));if(ok&&result.Get()&&*static_cast<bool*>(api.object_unbox(result.Get())))return;}
            actor.visuals.erase(std::remove_if(actor.visuals.begin(),actor.visuals.end(),[&](const auto& n){void* args[]={t};bool ok=false;Root result(api,api.Call(is_child,n->object.Get(),args,&ok));return ok&&result.Get()&&*static_cast<bool*>(api.object_unbox(result.Get()));}),actor.visuals.end());
            actor.visuals.push_back(std::make_unique<ScaleNode>(api,t));
        };
        if(Alive(actor.animator.Get())){Root t(api,api.Call(transform,actor.animator.Get()));add_scale(t.Get());}
        for(const auto* field:visual_fields){Root object(api,Ref(actor.object.Get(),field));if(!Alive(object.Get()))continue;scan(object.Get());Root t(api,api.Call(go_transform,object.Get()));add_scale(t.Get());}
    }
    void FreezeAnimations(Actor& actor,bool paused){
        for(auto& n:actor.animations){if(!paused&&!n->frozen)continue;if(!Alive(n->object.Get()))continue;
            if(paused){if(!n->frozen)n->enabled=api.Value<bool>(enabled_get,n->object.Get(),false);if(Set(enabled_set,n->object.Get(),false))n->frozen=true;}
            else if(n->frozen){if(api.Value<bool>(enabled_get,n->object.Get(),true)||Set(enabled_set,n->object.Get(),n->enabled))n->frozen=false;}
        }
    }
    void ScaleVisuals(Actor& actor,float factor){
        for(auto& n:actor.visuals){if(Near(factor,1)&&!n->changed)continue;if(!Alive(n->object.Get()))continue;const Vec3 now=api.Value<Vec3>(scale_get,n->object.Get());if(!Finite(now))continue;
            if(Near(factor,1)){if(n->changed&&Near(now,n->last))Set(scale_set,n->object.Get(),n->before);n->changed=false;continue;}
            if(!n->changed||!Near(now,n->last))n->before=now;
            const Vec3 value{n->before.x*factor,n->before.y*factor,n->before.z*factor};
            if(Finite(value)&&Set(scale_set,n->object.Get(),value)){n->last=value;n->changed=true;}
        }
    }
    void FloatFields(Actor& actor,const std::array<const char*,3>& names,std::vector<Scalar>& result){
        for(const auto* name:names){
            if(!name)continue;
            auto* f=api.class_get_field_from_name(api.object_get_class(actor.object.Get()),name);if(!f)continue;
            auto* type=api.field_get_type(f);auto* c=type?api.class_from_type(type):nullptr;
            if(!c||std::strcmp(api.class_get_namespace(c),"System")||std::strcmp(api.class_get_name(c),"Single"))continue;
            float v=0;api.field_get_value(actor.object.Get(),f,&v);if(std::isfinite(v)&&std::fabs(v)<10000)result.push_back({f,v,v,false});
        }
    }
    void ScalarApply(Actor& a,std::vector<Scalar>& fields,float factor){
        for(auto& f:fields){
            float now=0;api.field_get_value(a.object.Get(),f.field,&now);if(!std::isfinite(now))continue;
            if(!f.changed||!Near(now,f.last))f.base=now;
            const float desired=f.base*factor;
            if(!std::isfinite(desired)||std::fabs(desired)>10000)continue;
            if(Near(factor,1.f)){if(f.changed&&Near(now,f.last))api.field_set_value(a.object.Get(),f.field,&f.base);f.changed=false;}
            else{float value=desired;api.field_set_value(a.object.Get(),f.field,&value);f.last=value;f.changed=true;}
        }
    }
    void Restore(Actor& a){
        a.pose_frozen=a.local_pose_frozen=false;
        FreezeAnimations(a,false);ScaleVisuals(a,1);
        const bool fields_changed=std::any_of(a.speed_fields.begin(),a.speed_fields.end(),[](const auto& f){return f.changed;})||
            std::any_of(a.animation_fields.begin(),a.animation_fields.end(),[](const auto& f){return f.changed;});
        if(!fields_changed&&!a.nav_changed&&!a.anim_changed&&!a.scale_changed)return;
        if(!Alive(a.object.Get()))return;
        ScalarApply(a,a.speed_fields,1);ScalarApply(a,a.animation_fields,1);
        // Never disable the AI to pause it: OnDisable unregisters game events
        // and may terminate coroutines. The update hook freezes its decision loop.
        if(Alive(a.nav.Get())&&api.Value<bool>(enabled_get,a.nav.Get(),false)){
            if(a.nav_changed){if(!Near(api.Value<float>(nav_get,a.nav.Get(),-1),a.nav_last)||Set(nav_set,a.nav.Get(),a.nav_base))a.nav_changed=false;}
        }
        if(Alive(a.animator.Get())&&a.anim_changed&&Near(api.Value<float>(anim_get,a.animator.Get(),-1),a.anim_last))Set(anim_set,a.animator.Get(),a.anim_base);
        if(Alive(a.transform.Get())&&a.scale_changed&&Near(api.Value<Vec3>(scale_get,a.transform.Get()),a.scale_last))Set(scale_set,a.transform.Get(),a.scale_before);
        a.anim_changed=a.scale_changed=false;
    }
    Enemy EffectiveSettings(const Actor& actor){
        Enemy result=settings[actor.kind];
        for(const auto& clone:clones)if(clone->custom_settings&&clone->controller.Get()==actor.object.Get()){
            result.speed=clone->settings.speed;result.size=clone->settings.size;break;
        }
        return result;
    }
    void Apply(Actor& a){
        const auto cfg=EffectiveSettings(a);
        if(!cfg.paused&&Near(cfg.speed,1)&&Near(cfg.animation,1)&&Near(cfg.size,1)){Restore(a);return;}
        if(!Alive(a.object.Get()))return;
        if(!Alive(a.nav.Get())&&specs[a.kind].nav)a.nav.Set(Ref(a.object.Get(),specs[a.kind].nav));
        ScalarApply(a,a.speed_fields,cfg.paused?0.f:cfg.speed);ScalarApply(a,a.animation_fields,cfg.animation);
        FreezeAnimations(a,cfg.paused);ScaleVisuals(a,cfg.size);
        if(cfg.paused&&Alive(a.transform.Get())){
            if(!a.pose_frozen)a.pose_frozen=Pose(a.transform.Get(),a.frozen_position,a.frozen_rotation);
            if(!a.local_pose_frozen&&local_pos_get&&local_rot_get){
                bool p_ok=false,q_ok=false;Root p(api,api.Call(local_pos_get,a.transform.Get(),nullptr,&p_ok)),q(api,api.Call(local_rot_get,a.transform.Get(),nullptr,&q_ok));
                if(p_ok&&q_ok&&p.Get()&&q.Get()){
                    a.frozen_local_position=*static_cast<Vec3*>(api.object_unbox(p.Get()));a.frozen_local_rotation=*static_cast<Quat*>(api.object_unbox(q.Get()));a.local_pose_frozen=Finite(a.frozen_local_position);
                }
            }
            if(a.pose_frozen){Set(pos_set,a.transform.Get(),a.frozen_position);Set(rot_set,a.transform.Get(),a.frozen_rotation);}
        }else a.pose_frozen=a.local_pose_frozen=false;
        if(Alive(a.nav.Get())&&nav_get&&nav_set&&api.Value<bool>(enabled_get,a.nav.Get(),false)){
            const float now=api.Value<float>(nav_get,a.nav.Get(),-1);
            if(std::isfinite(now)&&now>=0){
                if(!a.nav_changed||!Near(now,a.nav_last))a.nav_base=now;
                const float value=cfg.paused?0.f:std::clamp(a.nav_base*cfg.speed,0.f,100.f);
                if(Set(nav_set,a.nav.Get(),value)){a.nav_last=value;a.nav_changed=true;}
            }
        }
        if(Alive(a.animator.Get())&&anim_get&&anim_set){
            const float now=api.Value<float>(anim_get,a.animator.Get(),-1);
            if(std::isfinite(now)&&now>=0){
                if(!a.anim_changed||!Near(now,a.anim_last))a.anim_base=now;
                const float value=cfg.paused?0.f:std::clamp(a.anim_base*cfg.animation,0.f,20.f);
                if(Set(anim_set,a.animator.Get(),value)){a.anim_last=value;a.anim_changed=true;}
            }
        }
        if(a.visuals.empty()&&Alive(a.transform.Get())){
            const Vec3 now=api.Value<Vec3>(scale_get,a.transform.Get());
            if(Finite(now)){
                if(!a.scale_changed)a.scale_before=now;
                const Vec3 value{a.scale_before.x*cfg.size,a.scale_before.y*cfg.size,a.scale_before.z*cfg.size};
                if(Finite(value)&&Set(scale_set,a.transform.Get(),value)){a.scale_last=value;a.scale_changed=true;}
            }
        }
    }
    void Discover(){
        for(size_t k=0;k<kEnemyCount;++k){
            snap.enemies[k].count=0;
            Each(classes[k],[&](Object* o){
                Root object_go(api,api.Call(go,o));if(!Alive(object_go.Get())||!api.Value<bool>(go_active,object_go.Get(),false))return;
                ++snap.enemies[k].count;
                if(actors.size()>=48)return;
                if(std::any_of(actors.begin(),actors.end(),[&](const auto& a){return a->object.Get()==o;}))return;
                auto actor=std::make_unique<Actor>(api,o,k);
                actor->nav.Set(Ref(o,specs[k].nav));actor->animator.Set(Ref(o,specs[k].animator));actor->transform.Set(api.Call(transform,o));
                FloatFields(*actor,specs[k].speed,actor->speed_fields);FloatFields(*actor,specs[k].animation,actor->animation_fields);
                BuildVisuals(*actor);
                actors.push_back(std::move(actor));
            });
        }
        actors.erase(std::remove_if(actors.begin(),actors.end(),[&](const auto& a){return !Alive(a->object.Get());}),actors.end());
    }
    void Catalog(){
        prefabs.clear();snap.items.clear();if(!inventory.Get()||!tie_class)return;
        Root ties(api,Ref(inventory.Get(),"handItemTies"));if(!ties.Get())return;
        const int n=api.Value<int>(length,ties.Get(),-1);if(n<0||n>128)return;
        auto* id_field=api.class_get_field_from_name(tie_class,"pickUpObject");auto* prefab_field=api.class_get_field_from_name(tie_class,"handObjectPrefab");
        if(!id_field||!prefab_field)return;
        const auto* enum_type=api.field_get_type(id_field);auto* enum_class=enum_type?api.class_from_type(enum_type):nullptr;
        const auto* prefab_type=api.field_get_type(prefab_field);auto* prefab_class=prefab_type?api.class_from_type(prefab_type):nullptr;
        if(!enum_class||std::strcmp(api.class_get_name(enum_class),"PickUpObject")||!prefab_class||std::strcmp(api.class_get_name(prefab_class),"GameObject"))return;
        std::array<bool,256> seen{},ambiguous{};
        for(int i=0;i<n;++i){
            void* at[]={&i};bool ok=false;Root tie(api,api.Call(item,ties.Get(),at,&ok));if(!ok||!tie.Get()||api.object_get_class(tie.Get())!=tie_class)continue;
            uint64_t stored_id=0;Object* prefab=nullptr;api.field_get_value(tie.Get(),id_field,&stored_id);api.field_get_value(tie.Get(),prefab_field,&prefab);
            Root rooted(api,prefab);if(!stored_id||stored_id>255||!Alive(rooted.Get()))continue;
            const auto id=static_cast<uint8_t>(stored_id);
            Root text(api,api.Call(name_get,rooted.Get(),nullptr,&ok));if(!ok||!text.Get())continue;
            const auto name=api.Utf8(reinterpret_cast<String*>(text.Get()));if(name.empty()||name.size()>160)continue;
            if(seen[id]){ambiguous[id]=true;continue;}seen[id]=true;
            prefabs.push_back(std::make_unique<Prefab>(api,id,rooted.Get(),name));
        }
        prefabs.erase(std::remove_if(prefabs.begin(),prefabs.end(),[&](const auto& p){return ambiguous[static_cast<size_t>(p->id)];}),prefabs.end());
        AddAmmo();
        for(const auto& p:prefabs)snap.items.push_back({p->id,gameplay_catalog::Name(p->id),true});
        if(std::none_of(prefabs.begin(),prefabs.end(),[](const auto& p){return p->id==48;}))snap.items.push_back({48,gameplay_catalog::Name(48),false});
        std::sort(snap.items.begin(),snap.items.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    }
    Object* PathFor(const std::string& value){
        bool ok=false;Root prefix(api,api.Call(prefix_get,nullptr,nullptr,&ok));if(!ok||!prefix.Get())return nullptr;
        Root name(api,reinterpret_cast<Object*>(api.string_new(value.c_str())));if(!name.Get())return nullptr;
        void* args[]={prefix.Get(),name.Get()};return api.Call(path_combine,nullptr,args);
    }
    bool IsAmmo(Object* pickup){
        if(!pickup)return false;
        auto* field=api.class_get_field_from_name(api.object_get_class(pickup),"handObject");
        if(!FieldType(api,field,"","PickUpObject"))return false;
        uint64_t id=0;api.field_get_value(pickup,field,&id);return id==48;
    }
    bool HasAmmo(Object* object){bool found=false;return Children(object,pickup_class,[&](Object* pickup){found=IsAmmo(pickup)||found;})&&found;}
    void AddAmmo(){
        std::vector<std::unique_ptr<Root>> sources;
        const auto add=[&](Object* object){if(!Alive(object)||sources.size()>=64||std::any_of(sources.begin(),sources.end(),[&](const auto& p){return p->Get()==object;}))return;sources.push_back(std::make_unique<Root>(api,object));};
        for(const auto& p:prefabs)if(p->id==48)add(p->object.Get());
        prefabs.erase(std::remove_if(prefabs.begin(),prefabs.end(),[](const auto& p){return p->id==48;}),prefabs.end());
        Root linked(api,Ref(inventory.Get(),"shotgunManager")),linked_source(api,Ref(linked.Get(),"moreAmmo"));add(linked_source.Get());
        Each(shotgun_class,[&](Object* o){Root source(api,Ref(o,"moreAmmo"));add(source.Get());});
        AllOfType(pickup_class,[&](Object* pickup){
            if(!IsAmmo(pickup))return;
            Root object(api,api.Call(go,pickup));add(object.Get());
            Root pv(api,Ref(pickup,"pv")),owner(api,pv.Get()?api.Call(go,pv.Get()):nullptr);add(owner.Get());
        });
        for(const auto& source:sources){
            Root text(api,api.Call(name_get,source->Get()));if(!text.Get())continue;
            std::string name=api.Utf8(reinterpret_cast<String*>(text.Get()));if(name.empty()||name.size()>160)continue;
            const std::string suffix="(Clone)";if(name.size()>suffix.size()&&name.compare(name.size()-suffix.size(),suffix.size(),suffix)==0)name.resize(name.size()-suffix.size());
            Root asset(api);
            if(resources_load&&prefix_get&&path_combine){Root path(api,PathFor(name));auto* type=api.Find("UnityEngine","GameObject");Root reflection(api,type?api.type_get_object(api.class_get_type(type)):nullptr);
                if(path.Get()&&reflection.Get()){void* args[]={path.Get(),reflection.Get()};asset.Set(api.Call(resources_load,nullptr,args));}
            }
            if(Alive(asset.Get())&&HasAmmo(asset.Get())){bool has_view=false;if(Children(asset.Get(),view_class,[&](Object*){has_view=true;})&&has_view){prefabs.push_back(std::make_unique<Prefab>(api,48,asset.Get(),name));return;}}
        }
        // Static scene ammo without a registered network prefab is deliberately
        // unavailable. Never substitute a local visual that other clients cannot see.
    }
    Object* Create(Prefab& prefab,Vec3 position,Quat rotation,bool& ok,bool room_object=false){
        ok=false;
        Root path(api,PathFor(prefab.name));if(!path.Get())return nullptr;
        uint8_t group=0;void* args[]={path.Get(),&position,&rotation,&group,nullptr};return api.Call(room_object?room_spawn:spawn,nullptr,args,&ok);
    }
    void Spawn(int id){
        if(!snap.spawn_ready){snap.status=snap.spawn_status;return;}
        const auto it=std::find_if(prefabs.begin(),prefabs.end(),[&](const auto& p){return p->id==id;});
        if(it==prefabs.end()){snap.status=id==48?u8"Shotgun Ammo için doğrulanmış ağ prefabı yok; oluşturma kapalı.":u8"Eşya kaynağı bulunamadı.";return;}
        if(!InHierarchy(inventory.Get())||!Owned(inventory.Get(),"pv")){snap.status=u8"Eşya seçimi bu oyuncu oturumuna ait değil.";return;}
        if(id<47&&hand_get&&hand_pick){
            const uint8_t current=api.Value<uint8_t>(hand_get,inventory.Get(),255);
            if(current==255){snap.status=u8"Eldeki eşya okunamadı; işlem bekletildi.";return;}
            if(current!=id){
                uint8_t requested=static_cast<uint8_t>(id);void* args[]={&requested,nullptr};bool ok=false;
                api.Call(hand_pick,inventory.Get(),args,&ok);
                const uint8_t after=api.Value<uint8_t>(hand_get,inventory.Get(),255);
                snap.status=after==requested?(current?u8"Eldeki farklı eşya bırakıldı, seçtiğin eşya eline geldi.":u8"Seçtiğin eşya eline geldi."):u8"Oyun eşyanın ele alınmasını doğrulayamadı.";
                return;
            }
        }
        Root point(api,Ref(inventory.Get(),"dropPointTransform"));if(!Alive(point.Get())||!Alive((*it)->object.Get()))return;
        bool ok=false;
        Root pos(api,api.Call(pos_get,point.Get(),nullptr,&ok));if(!ok||!pos.Get())return;const Vec3 position=*static_cast<Vec3*>(api.object_unbox(pos.Get()));
        Root rot(api,api.Call(rot_get,point.Get(),nullptr,&ok));if(!ok||!rot.Get())return;const Quat rotation=*static_cast<Quat*>(api.object_unbox(rot.Get()));
        if(!Finite(position)||!std::isfinite(rotation.x)||!std::isfinite(rotation.y)||!std::isfinite(rotation.z)||!std::isfinite(rotation.w))return;
        Vec3 target=position;Quat facing=rotation;
        if(id==48&&InFront(target,facing,1.8f)&&nav_sample){NavHit floor{};float radius=1.8f;int areas=-1;void* args[]={&target,&floor,&radius,&areas};bool found_floor=false;Root result(api,api.Call(nav_sample,nullptr,args,&found_floor));const float dx=floor.position.x-target.x,dz=floor.position.z-target.z;if(found_floor&&result.Get()&&*static_cast<bool*>(api.object_unbox(result.Get()))&&floor.hit&&Finite(floor.position)&&dx*dx+dz*dz<.75f*.75f)target.y=floor.position.y+.05f;}
        Root created(api,Create(**it,target,facing,ok));
        snap.status=ok&&Alive(created.Get())?u8"Eşya önünde oluşturuldu; normal ağ oluşturma yolu kullanıldı.":u8"Oyun bu prefabı oluşturamadı; konum veya kaynak kaydı hazır olmayabilir.";
    }
    void Remove(size_t kind){
        int removed=0,local=0;
        for(auto& a:actors){
            if(removed>=8)break;
            if(a->kind!=kind||!Alive(a->object.Get()))continue;
            Root target(api,nullptr);
            const bool network=specs[kind].pv&&Owned(a->object.Get(),specs[kind].pv)&&network_destroy;
            if(network){
                Root pv(api,Ref(a->object.Get(),specs[kind].pv));target.Set(api.Call(go,pv.Get()));
            }else target.Set(api.Call(go,a->object.Get()));
            if(!Alive(target.Get()))continue;
            Restore(*a);
            if(!Alive(target.Get())||(network&&!Owned(a->object.Get(),specs[kind].pv)))continue;
            void* args[]={target.Get()};bool ok=false;
            api.Call(network?network_destroy:local_destroy,nullptr,args,&ok);if(ok){++removed;if(!network)++local;}
        }
        settings[kind]={};
        snap.status=removed?(local?u8"Yerel kaldırma çağrısı yapıldı; diğer oyuncularda nesne kalabilir.":u8"Sahibi olduğun düşman için ağdan kaldırma çağrısı yapıldı; ölüm/ödül akışı çalıştırılmaz."):u8"Kaldırılabilecek düşman bulunamadı.";
        next_scan=0;
    }
    bool BoolField(Object* o,const char* name,bool value){
        if(!o)return false;
        auto* f=api.class_get_field_from_name(api.object_get_class(o),name);if(!f)return false;
        auto* c=api.class_from_type(api.field_get_type(f));if(!c||std::strcmp(api.class_get_namespace(c),"System")||std::strcmp(api.class_get_name(c),"Boolean"))return false;
        api.field_set_value(o,f,&value);return true;
    }
    bool ReadBool(Object* o,Field* field,bool fallback=false){bool value=fallback;if(o&&field)api.field_get_value(o,field,&value);return value;}
    Object* BodyTransform(){Root controller(api,Ref(movement.Get(),"controller"));return controller.Get()?api.Call(transform,controller.Get()):nullptr;}
    bool Pose(Object* t,Vec3& p,Quat& q){
        if(!Alive(t))return false;
        bool ok=false;Root position(api,api.Call(pos_get,t,nullptr,&ok));if(!ok||!position.Get())return false;p=*static_cast<Vec3*>(api.object_unbox(position.Get()));
        Root rotation(api,api.Call(rot_get,t,nullptr,&ok));if(!ok||!rotation.Get())return false;q=*static_cast<Quat*>(api.object_unbox(rotation.Get()));
        return Finite(p)&&std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&std::isfinite(q.w);
    }
    bool InFront(Vec3& position,Quat& rotation,float distance=2){
        if(!InHierarchy(movement.Get())||!Owned(movement.Get(),"pv"))return false;
        Root body(api,BodyTransform()),camera(api,Ref(movement.Get(),"cameraTrans"));Vec3 p{},ignored{};Quat body_q{},camera_q{};
        if(!Pose(body.Get(),p,body_q))return false;
        if(!Pose(camera.Get(),ignored,camera_q)||!FrontPose(p,camera_q,distance,position,rotation))return FrontPose(p,body_q,distance,position,rotation);
        return true;
    }
    bool ReturnGhost(){
        if(!snap.ghost_ready||!ghost_active)return false;
        Root body(api,BodyTransform());Vec3 p{};Quat q{};if(!Pose(body.Get(),p,q))return false;
        p=ghost_origin;
        if(!Finite(p)||!Set(pos_set,body.Get(),p))return false;
        ghost_height=p.y;ghost_height_ready=true;
        snap.ghost=false;
        return true;
    }
    void ResetVerticalVelocity(){
        if(!movement_velocity||!Alive(movement.Get()))return;
        Vec3 v{};api.field_get_value(movement.Get(),movement_velocity,&v);if(Finite(v)){v.y=0;api.field_set_value(movement.Get(),movement_velocity,&v);}
    }
    bool SightOwned(){
        if(!sight_hooks_ready||!WorldMatches()||!InHierarchy(sight.Get())||!Owned(sight.Get(),"pv"))return false;
        Root pv(api,Ref(sight.Get(),"pv")),own(api,Ref(inventory.Get(),"pv"));
        if(!pv.Get()||!own.Get()||pv.Get()!=own.Get())return false;
        Root target(api,Ref(sight.Get(),"grannySeeTarget")),owner_t(api,api.Call(transform,sight.Get()));
        Root target_t(api,target.Get()?api.Call(go_transform,target.Get()):nullptr);
        return Alive(target.Get())&&Alive(owner_t.Get())&&Alive(target_t.Get())&&owner_t.Get()!=target_t.Get()&&ChildOf(target_t.Get(),owner_t.Get());
    }
    bool SendSight(bool on,bool network){
        const auto* method=api.FindMethod(sight_class,"hpg",2);if(!method)return false;
        struct Internal{bool& flag;bool saved;explicit Internal(bool& f):flag(f),saved(f){flag=true;}~Internal(){flag=saved;}} internal(sight_internal);
        void* args[]={&on,&network};bool ok=false;api.Call(method,sight.Get(),args,&ok);
        if(ok&&network)++sight_sends;
        return ok;
    }
    void ReleaseSight(bool restore){
        if(sight_applied&&restore&&SightOwned()&&Ref(sight.Get(),"grannySeeTarget")==sight_target.Get()){
            if(sight_restore_pending&&gameplay_bridge::Now()<sight_next)return;
            const bool local=SendSight(sight_normal,false),network=SendSight(sight_normal,true);
            __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG sight restore=%d local=%d network_call=%d",sight_normal,local,network);
            if(!local||!network){sight_restore_pending=true;sight_next=gameplay_bridge::Now()+1;return;}
        }
        sight_applied=sight_restore_pending=false;sight_next=0;sight_target.Set(nullptr);snap.invisible=false;
    }
    bool OverrideSightRequest(Object* o,bool requested,bool remember){
        if(sight_internal||o!=sight.Get()||!sight_applied||!snap.ready||!snap.god||!snap.god_ready||!SightOwned())return false;
        if(remember)sight_normal=requested;
        return true;
    }
    void UpdateSight(){
        snap.invisible_ready=SightOwned();
        if(sight_applied&&Ref(sight.Get(),"grannySeeTarget")!=sight_target.Get())ReleaseSight(false);
        if(!snap.god||!snap.god_ready||!snap.ready||!snap.invisible_ready){ReleaseSight(true);snap.invisible_status=snap.invisible_ready?u8"AI görünmezliği God Mode ile açılır.":u8"AI görünmezliği: kendi hedef bağlantın bekleniyor.";return;}
        if(sight_restore_pending){sight_restore_pending=false;sight_next=0;}
        const double now=gameplay_bridge::Now();
        if(!sight_applied){
            sight_target.Set(Ref(sight.Get(),"grannySeeTarget"));
            bool ok=false;Root state(api,api.Call(active_get,sight_target.Get(),nullptr,&ok));
            if(!ok||!state.Get())return;
            sight_normal=*static_cast<bool*>(api.object_unbox(state.Get()));sight_applied=true;sight_next=0;
        }
        // Immediate local application plus the game's own unbuffered RPC.
        // Periodic refresh covers late peers and host changes without a frame flood.
        const bool visible=api.Value<bool>(active_get,sight_target.Get(),true);
        if(visible||now>=sight_next){
            const bool local=SendSight(false,false),network=SendSight(false,true);sight_next=now+(local&&network?3:1);
            __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG sight hidden=1 local=%d network_call=%d sends=%u",local,network,sight_sends);
        }
        snap.invisible=!api.Value<bool>(active_get,sight_target.Get(),true);
        snap.invisible_status=snap.invisible?u8"AI görünmezliği açık; ağ güncellemesi gönderiliyor.":u8"AI hedefinin kapanması bekleniyor.";
    }
    void AmmoVisuals(bool enabled){
        if(!enabled&&!cross_visual&&!shot_visual)return;
        if(!Alive(crossbow.Get()))cross_visual=false;
        if(!Alive(shotgun.Get()))shot_visual=false;
        const uint8_t held=inventory.Get()&&hand_get?api.Value<uint8_t>(hand_get,inventory.Get(),0):0;
        const bool cross_on=enabled&&held==9&&InHierarchy(crossbow.Get())&&Owned(crossbow.Get(),"pv");
        if(cross_on||cross_visual){
            Root button(api,Ref(crossbow.Get(),"shootButton"));if(Alive(button.Get())){if(cross_on&&!cross_visual)cross_button_saved=api.Value<bool>(active_get,button.Get(),false);Set(active_set,button.Get(),cross_on||(held==9&&cross_button_saved));}
            const bool loaded=cross_on||ReadBool(crossbow.Get(),crossbow_loaded);Set(cross_images,crossbow.Get(),loaded);Set(cross_arrow,crossbow.Get(),loaded);cross_visual=cross_on;
        }
        const bool shot_on=enabled&&held==25&&InHierarchy(shotgun.Get())&&Owned(shotgun.Get(),"pv");
        if(shot_on||shot_visual){
            Root button(api,Ref(shotgun.Get(),"shotgunShootButton"));if(Alive(button.Get())){if(shot_on&&!shot_visual)shot_button_saved=api.Value<bool>(active_get,button.Get(),false);Set(active_set,button.Get(),shot_on||(held==25&&shot_button_saved));}
            Root manager(api,Ref(shotgun.Get(),"shotgunManager")),ammo(api,Ref(shotgun.Get(),"shotgunHandAmmo"));
            const bool loaded=shot_on||ReadBool(manager.Get(),shotgun_loaded);if(Alive(ammo.Get()))Set(active_set,ammo.Get(),loaded);shot_visual=shot_on;
        }
    }
    void ReleasePlayerSpeed(bool restore){
        if(restore&&WorldMatches()&&InHierarchy(speed_owner.Get())&&Owned(speed_owner.Get(),"pv"))for(auto& f:player_speeds){
            float current=0;api.field_get_value(speed_owner.Get(),f.field,&current);
            if(f.changed&&Near(current,f.last))api.field_set_value(speed_owner.Get(),f.field,&f.base);
        }
        player_speeds.clear();speed_owner.Set(nullptr);snap.player_speed_ready=false;snap.player_speed=1;
    }
    bool PlayerSpeedReady(){
        if(!snap.ready||!InHierarchy(movement.Get())||!Owned(movement.Get(),"pv"))return false;
        for(const char* name:{"walkSpeed","crouchSpeed"}){
            auto* f=api.class_get_field_from_name(api.object_get_class(movement.Get()),name);if(!FieldType(api,f,"System","Single"))return false;
            float value=0;api.field_get_value(movement.Get(),f,&value);if(!std::isfinite(value)||value<=0||value>500)return false;
        }return true;
    }
    void UpdatePlayerSpeed(){
        snap.player_speed_ready=PlayerSpeedReady();
        if(!snap.player_speed_ready){ReleasePlayerSpeed(true);return;}
        if(speed_owner.Get()!=movement.Get()){
            const float requested=snap.player_speed;ReleasePlayerSpeed(true);snap.player_speed=requested;snap.player_speed_ready=true;speed_owner.Set(movement.Get());
            for(const char* name:{"walkSpeed","crouchSpeed"}){auto* f=api.class_get_field_from_name(api.object_get_class(movement.Get()),name);float v=0;api.field_get_value(movement.Get(),f,&v);player_speeds.push_back({f,v,v,false});}
        }
        for(auto& f:player_speeds){
            float current=0;api.field_get_value(speed_owner.Get(),f.field,&current);if(!std::isfinite(current)||current<=0)continue;
            if(!f.changed||!Near(current,f.last))f.base=current;
            const float value=f.base*snap.player_speed;
            if(snap.player_speed==1){if(f.changed&&Near(current,f.last))api.field_set_value(speed_owner.Get(),f.field,&f.base);f.changed=false;}
            else if(std::isfinite(value)&&value>0&&value<=500){float next=value;api.field_set_value(speed_owner.Get(),f.field,&next);f.last=next;f.changed=true;}
        }
    }
    void DisableTools(bool restore){
        ReleasePlayerSpeed(restore);
        ghost_enabled=ammo_enabled=false;ghost_height_ready=false;
        if(restore){if(ghost_active)ResetVerticalVelocity();AmmoVisuals(false);}
        ghost_active=cross_visual=shot_visual=false;
    }
    void UpdateTools(){
        UpdatePlayerSpeed();
        gameplay_bridge::SyncGodAmmo(snap);
        UpdateSight();
        if(snap.ghost&&!ghost_active){Root body(api,BodyTransform());Quat q{};if(Pose(body.Get(),ghost_origin,q)){ghost_active=true;ghost_height=ghost_origin.y;ghost_height_ready=true;ResetVerticalVelocity();}else{snap.ghost=false;snap.status=u8"Hayalet modu başlangıç konumunu okuyamadı.";}}
        if(!snap.ghost&&ghost_active){ResetVerticalVelocity();ghost_active=false;ghost_height_ready=false;}
        if(snap.ghost&&ghost_active&&snap.ghost_ready){
            ResetVerticalVelocity();Root body(api,BodyTransform());Vec3 p{};Quat q{};
            if(ghost_height_ready&&Pose(body.Get(),p,q)&&!Near(p.y,ghost_height)){p.y=ghost_height;Set(pos_set,body.Get(),p);}
        }
        AmmoVisuals(snap.infinity&&snap.infinity_ready);
        ghost_enabled=snap.ghost&&ghost_active&&snap.ghost_ready;ammo_enabled=protect&&snap.infinity;
    }
    float PositiveField(Object* o,const char* name){
        auto* field=api.class_get_field_from_name(api.object_get_class(o),name);
        if(!FieldType(api,field,"System","Single"))return 0;
        float value=0;api.field_get_value(o,field,&value);return std::isfinite(value)&&value>0&&value<100?value:0;
    }
    bool Flag(Object* object,const char* name){
        if(!object)return false;
        auto* f=api.class_get_field_from_name(api.object_get_class(object),name);
        return FieldType(api,f,"System","Boolean")&&ReadBool(object,f);
    }
    bool ReleaseHiddenTarget(Actor& actor){
        if(actor.kind>2||settings[actor.kind].paused||!snap.authority||!WorldMatches()||!api.Value<bool>(master,nullptr,false)||!InHierarchy(actor.object.Get())||!Owned(actor.object.Get(),specs[actor.kind].pv)||!go_active)return false;
        auto* ai=actor.object.Get();
        for(const char* name:{"hitByArrow","hitByGun","hitByCar","hitByPepper","hitByPepperStart","bastuKilled","freeze","gotHitByWeapon"})if(Flag(ai,name))return false;
        Root eye(api,actor.kind<2?Ref(ai,"enemyEye"):ai),selected(api,Ref(eye.Get(),actor.kind==0?"pka":actor.kind==1?"pdz":"rnx"));
        if(!selected.Get())return false;
        Root target(api,Ref(ai,"targetTransform"));if(!target.Get())target.Set(Ref(ai,"target"));
        Root target_go(api,target.Get()?api.Call(go,target.Get()):nullptr);
        const bool local_hidden=snap.god&&snap.god_ready&&snap.invisible&&selected.Get()==api.Call(local_player_method);
        if(!local_hidden&&(!Alive(target_go.Get())||api.Value<bool>(go_active,target_go.Get(),true)))return false;
        const auto* clear=api.FindMethod(api.object_get_class(eye.Get()),actor.kind==0?"eli":actor.kind==1?"dwz":"inb",0);
        const auto* reset=api.FindMethod(api.object_get_class(ai),actor.kind==0?"eim":actor.kind==1?"duo":"inf",0);
        const auto* route=actor.kind<2?api.FindMethod(api.object_get_class(ai),actor.kind==0?"ein":"dup",0):nullptr;
        if(!clear||!reset||(actor.kind<2&&!route))return false;
        // Use native per-instance target/animation cleanup, then let native
        // perception choose a different visible player. No scene reinitialization.
        bool ok=false;api.Call(clear,eye.Get(),nullptr,&ok);if(!ok)return false;
        if(actor.kind<2){
            api.Call(reset,ai,nullptr,&ok);if(!ok)return false;
            for(const char* name:{"seePlayer","playerGetCaught","attackingPlayer","startAttack","grannyGonnaSmack","inTrigger","aboutKillingCollider","startWalk","stopWalk"})BoolField(ai,name,false);
            BoolField(ai,"GrannyMoving",true);SetReference(ai,"targetTransform",nullptr,"Transform");
            api.Call(route,ai,nullptr,&ok);
        }else{BoolField(ai,"eatingPlayer",false);api.Call(reset,ai,nullptr,&ok);}
        actor.attack_release=0;++target_releases;
        __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG ai release-hidden kind=%zu native_ok=%d total=%u",actor.kind,ok,target_releases);
        return true;
    }
    void CombatGuard(Actor& actor,bool enabled){
        if(ReleaseHiddenTarget(actor))return;
        // Let the original attack play. A protected player's missing death
        // callback otherwise leaves this enemy permanently caught/eating.
        if(!enabled){actor.attack_release=0;return;}
        if(actor.kind>2||settings[actor.kind].paused||!snap.authority||!api.Value<bool>(master,nullptr,false)||!WorldMatches()||!Alive(actor.object.Get())){actor.attack_release=0;return;}
        Root body(api,BodyTransform());if(!Alive(body.Get()))return;
        Root eye(api,actor.kind<2?Ref(actor.object.Get(),"enemyEye"):nullptr);
        Root selected(api,actor.kind<2?Ref(eye.Get(),actor.kind==0?"pka":"pdz"):Ref(actor.object.Get(),"rnx"));
        Root local(api,api.Call(local_player_method));
        if(selected.Get()&&selected.Get()!=local.Get()){actor.attack_release=0;return;}
        Root target(api,Ref(actor.object.Get(),"targetTransform"));if(!target.Get())target.Set(Ref(actor.object.Get(),"target"));
        const bool own=selected.Get()?selected.Get()==local.Get():Alive(target.Get())&&ChildOf(target.Get(),body.Get());
        if(!own){actor.attack_release=0;return;}
        for(const char* name:{"hitByArrow","hitByGun","hitByCar","bastuKilled","freeze","gotHitByWeapon"})
            if(Flag(actor.object.Get(),name)){actor.attack_release=0;return;}
        const bool attacking=actor.kind<2?(Flag(actor.object.Get(),"playerGetCaught")||Flag(actor.object.Get(),"attackingPlayer")):Flag(actor.object.Get(),"eatingPlayer");
        if(!attacking){actor.attack_release=0;return;}
        const double now=gameplay_bridge::Now();
        if(!actor.attack_release){
            // AnimatorStateInfo getters were stripped. Use the actual victim
            // clip when available, with a bounded fallback, not a guessed ABI.
            float duration=3.5f;
            Root clip(api,Ref(script.Get(),"playerHurtAnim"));
            if(!clip.Get()&&actor.kind==2)clip.Set(Ref(actor.object.Get(),"momHitAnim"));
            if(clip.Get()&&clip_length){float length=api.Value<float>(clip_length,clip.Get(),0);if(std::isfinite(length)&&length>0)duration=std::clamp(length+.35f,3.5f,12.f);}
            actor.attack_release=now+duration;return;
        }
        if(now<actor.attack_release)return;
        actor.attack_release=0;
        if(actor.kind<2){
            for(const char* name:{"playerGetCaught","grannyGonnaSmack","attackingPlayer","startAttack","inTrigger","aboutKillingCollider","stopWalk","startWalk"})BoolField(actor.object.Get(),name,false);
            BoolField(actor.object.Get(),"GrannyMoving",true);
        }else BoolField(actor.object.Get(),"eatingPlayer",false);
        // Retain the selected player, eye, chase flags and destination. No
        // forced patrol, target erasure or global event is needed to resume.
        Root nav(api,Ref(actor.object.Get(),specs[actor.kind].nav));
        float speed=PositiveField(actor.object.Get(),actor.kind<2?"grannysVarSpeed":"chaseSpeed");
        if(speed<=0&&actor.kind==2)speed=PositiveField(actor.object.Get(),"wanderSpeed");
        if(speed>0)Set(nav_set,nav.Get(),speed);
    }
    void Kill(size_t kind){
        settings[kind]={};for(auto& a:actors)if(a->kind==kind)Restore(*a);
        bool sent=false;
        if(kind<2){
            const char* names[]={"RPC_SyncShotShotgunGranny","RPC_SyncShotShotgunGrandpa"};auto* object_class=api.Find("System","Object");
            if(rpc&&api.array_new&&object_class)Each(clone_managers[kind],[&](Object* o){
                if(sent||!InHierarchy(o)||!api.FindMethod(clone_managers[kind],names[kind],1))return;
                Root pv(api,Ref(o,"pv")),name(api,reinterpret_cast<Object*>(api.string_new(names[kind]))),parameters(api,api.array_new(object_class,0));
                if(!Alive(pv.Get())||!name.Get()||!parameters.Get())return;
                int target=2;void* args[]={name.Get(),&target,parameters.Get()};api.Call(rpc,pv.Get(),args,&sent);
            });
        }else if(kind==2){
            const auto* request=api.FindMethod(classes[2],"imv",2);
            for(auto& a:actors){if(a->kind!=2||!Alive(a->object.Get()))continue;void* args[]={nullptr,nullptr};bool ok=false;api.Call(request,a->object.Get(),args,&ok);sent=sent||ok;}
        }else if(kind==3){
            const auto* request=api.FindMethod(classes[3],"jek",2);
            for(auto& a:actors){if(a->kind!=3||!Alive(a->object.Get()))continue;
                Root pv(api,Ref(a->object.Get(),"pv"));if(!Alive(pv.Get()))continue;
                int id=api.Value<int>(view_id,pv.Get(),0);if(id<=0)continue;
                void* args[]={nullptr,&id};bool ok=false;api.Call(request,a->object.Get(),args,&ok);sent=sent||ok;
            }
        }else if(kind==4){
            for(auto& a:actors)if(a->kind==4&&Alive(a->object.Get()))sent=BoolField(a->object.Get(),"shootInBur",true)||sent;
        }
        snap.status=sent?(kind==4?u8"Karganın normal vurulma akışı istendi; uçuşu bitince işlenebilir. Etki yereldir.":u8"Oyunun normal vurulma/ölüm isteği gönderildi. Süreyi, ragdoll ve yeniden doğmayı oyun yönetir."):u8"Bu düşman için doğrulanmış ölüm isteği bulunamadı; Kaldır seçeneğini kullanabilirsin.";
        next_scan=0;
    }
    void Clone(size_t kind,const Enemy& configuration=Enemy{},float distance=4){
        clones.erase(std::remove_if(clones.begin(),clones.end(),[&](const auto& p){return !Alive(p->Get());}),clones.end());
        if(kind>=3||clones.size()>=8){snap.status=kind>=3?u8"Bu düşmanın doğrulanmış klon prefabı yok.":u8"Bu oturumda en fazla sekiz canlı klon oluşturulabilir.";return;}
        const char* fields[]={"grannyPrefab","grandpaPrefab","spiderMomPrefab"};
        Root prefab(api);bool ambiguous=false;
        Each(clone_managers[kind],[&](Object* o){if(!InHierarchy(o))return;Root value(api,Ref(o,fields[kind]));if(!Alive(value.Get()))return;if(prefab.Get()&&prefab.Get()!=value.Get())ambiguous=true;prefab.Set(value.Get());});
        if(!prefab.Get()||ambiguous){snap.status=u8"Klon prefabı bulunamadı veya belirsiz.";return;}
        if(!nav_sample){snap.status=u8"Düşman için zemin bağlantısı hazır değil.";return;}
        Root text(api,api.Call(name_get,prefab.Get()));if(!text.Get())return;
        const auto name=api.Utf8(reinterpret_cast<String*>(text.Get()));
        if(name.empty()||name.size()>160)return;
        Vec3 position{};Quat q{};if(!InFront(position,q,distance)){snap.status=u8"Kendi oyuncunun ön konumu okunamadı.";return;}
        NavHit hit{};float radius=1.8f;int areas=-1;void* sample[]={&position,&hit,&radius,&areas};bool sampled=false;Root found(api,api.Call(nav_sample,nullptr,sample,&sampled));
        const float dx=hit.position.x-position.x,dz=hit.position.z-position.z;
        if(!sampled||!found.Get()||!*static_cast<bool*>(api.object_unbox(found.Get()))||!hit.hit||!Finite(hit.position)||dx*dx+dz*dz>.35f*.35f){snap.status=u8"Tam önünde yürünebilir zemin bulunamadı; klon oluşturulmadı.";return;}
        position=hit.position;
        const bool own_master=api.Value<bool>(master,nullptr,false);
        Prefab source(api,0,prefab.Get(),name);bool ok=false;Root created(api,Create(source,position,q,ok,own_master&&room_spawn));
        if(!ok||!Alive(created.Get())){snap.status=u8"Oyun klon prefabını oluşturmayı reddetti.";return;}
        Root controller(api);bool ambiguous_controller=false;
        Children(created.Get(),classes[kind],[&](Object* o){if(controller.Get()&&controller.Get()!=o)ambiguous_controller=true;controller.Set(o);});
        // Serialized controller.pv may be initialized by Start next frame.
        // Validate the freshly instantiated hierarchy's actual registered view.
        Root pv(api);Children(created.Get(),view_class,[&](Object* view){if(!pv.Get()&&api.Value<int>(view_id,view,0)>0&&api.Value<bool>(mine,view,false))pv.Set(view);});
        const int id=pv.Get()?api.Value<int>(view_id,pv.Get(),0):0;
        if(!controller.Get()||ambiguous_controller||(!Alive(pv.Get())||id<=0||!api.Value<bool>(mine,pv.Get(),false))){
            void* args[]={created.Get()};api.Call(id>0&&api.Value<bool>(mine,pv.Get(),false)?network_destroy:local_destroy,nullptr,args);
            snap.status=u8"Klonun AI/ağ kimliği doğrulanamadı; oluşturulan nesne temizlendi.";return;
        }
        Root t(api,api.Call(go_transform,created.Get()));Set(pos_set,t.Get(),position);Set(rot_set,t.Get(),q);
        clones.push_back(std::make_unique<CloneRecord>(api,created.Get(),pv.Get(),position,gameplay_bridge::Now()));
        clones.back()->controller.Set(controller.Get());clones.back()->settings=configuration;clones.back()->custom_settings=true;clones.back()->kind=kind;
        Discover();for(auto& actor:actors)if(actor->object.Get()==controller.Get())Apply(*actor);
        snap.status=u8"Klon oluşturuldu. AI başlangıcı ve oda sahibiyle kontrol eşleşmesi bekleniyor.";next_scan=0;
    }
    bool SetReference(Object* object,const char* name,Object* value,const char* type,const char* namespc="UnityEngine"){
        if(!api.field_set_value_object||!object||(value&&!Alive(value)))return false;
        auto* field=api.class_get_field_from_name(api.object_get_class(object),name);
        if(!FieldType(api,field,namespc,type))return false;
        api.field_set_value_object(object,field,value);Object* after=nullptr;api.field_get_value(object,field,&after);return after==value;
    }
    bool Derives(Class* c,const char* namespc,const char* name){
        for(int i=0;c&&i<32;++i,c=api.class_get_parent?api.class_get_parent(c):nullptr)
            if(!std::strcmp(api.class_get_namespace(c),namespc)&&!std::strcmp(api.class_get_name(c),name))return true;
        return false;
    }
    bool ChildOf(Object* t,Object* root){
        if(!t||!root)return false;
        if(t==root)return true;
        void* args[]={root};return t&&root&&ChildOfCall(t,args);
    }
    bool ChildOfCall(Object* t,void** args){bool ok=false;Root v(api,api.Call(is_child,t,args,&ok));return ok&&v.Get()&&*static_cast<bool*>(api.object_unbox(v.Get()));}
    Object* MapCloneTransform(Object* original,Object* copy,Object* node){
        if(!parent_get||!child_count||!child_get||!name_get)return nullptr;
        Root cursor(api,node);std::vector<int> path;std::vector<std::string> labels;
        for(int depth=0;cursor.Get()!=original&&depth<32;++depth){
            Root parent(api,api.Call(parent_get,cursor.Get()));if(!Alive(parent.Get()))return nullptr;
            const int n=api.Value<int>(child_count,parent.Get(),-1);if(n<0||n>256)return nullptr;
            int index=-1;for(int i=0;i<n;++i){void* args[]={&i};Root child(api,api.Call(child_get,parent.Get(),args));if(child.Get()==cursor.Get()){index=i;break;}}
            if(index<0)return nullptr;
            Root label(api,api.Call(name_get,cursor.Get()));if(!label.Get())return nullptr;
            path.push_back(index);labels.push_back(api.Utf8(reinterpret_cast<String*>(label.Get())));cursor.Set(parent.Get());
        }
        if(cursor.Get()!=original)return nullptr;
        cursor.Set(copy);
        for(size_t i=path.size();i>0;--i){
            const int count=api.Value<int>(child_count,cursor.Get(),-1);if(count<0||count>256)return nullptr;
            Root child(api);int matches=0;
            for(int index=0;index<count;++index){void* args[]={&index};Root candidate(api,api.Call(child_get,cursor.Get(),args));
                Root name(api,Alive(candidate.Get())?api.Call(name_get,candidate.Get()):nullptr);
                if(name.Get()&&api.Utf8(reinterpret_cast<String*>(name.Get()))==labels[i-1]){++matches;child.Set(candidate.Get());}
            }
            // Scene/prefab siblings may differ; accept only one matching path.
            if(matches!=1)return nullptr;
            cursor.Set(child.Get());
        }return cursor.Get();
    }
    bool RepairCloneReferences(Object* source,Object* copy,Object* original_root,Object* copy_root){
        if(!api.class_get_fields||!api.field_get_name||!api.field_get_flags||!api.field_set_value_object||!get_component)return false;
        if(api.object_get_class(source)!=api.object_get_class(copy))return false;
        void* it=nullptr;
        for(int n=0;n<256;++n){auto* f=api.class_get_fields(api.object_get_class(source),&it);if(!f)break;
            if(api.field_get_flags(f)&(0x10|0x20|0x40))continue; // static, readonly, literal
            const char* name=api.field_get_name(f);if(!name)continue;
            if(!std::strcmp(name,"pv")||!std::strcmp(name,"target")||!std::strcmp(name,"targetTransform")||!std::strcmp(name,"playerPos")||!std::strcmp(name,"Player")||!std::strcmp(name,"player")||!std::strcmp(name,"playerFollowing")||!std::strcmp(name,"whoHasTeddy")||!std::strcmp(name,"localPlayerPosFollow"))continue;
            auto* type=api.class_from_type(api.field_get_type(f));if(!type)continue;
            if(Derives(type,"Photon.Pun","PhotonView"))continue; // never copy network identity
            const bool gameobject=Derives(type,"UnityEngine","GameObject"),component=Derives(type,"UnityEngine","Component");if(!gameobject&&!component)continue;
            Object *before=nullptr,*from=nullptr;api.field_get_value(copy,f,&before);api.field_get_value(source,f,&from);
            Root prior(api,before),value(api,from);if(!Alive(value.Get())||(Alive(prior.Get())&&prior.Get()!=value.Get()))continue;
            Root src_t(api,api.Call(gameobject?go_transform:transform,value.Get()));if(!Alive(src_t.Get()))continue;
            Root mapped(api);
            if(ChildOf(src_t.Get(),original_root)){
                Root dst_t(api,MapCloneTransform(original_root,copy_root,src_t.Get()));if(!Alive(dst_t.Get()))continue;
                auto* actual=api.object_get_class(value.Get());
                if(gameobject)mapped.Set(api.Call(go,dst_t.Get()));
                else if(Derives(actual,"UnityEngine","Transform"))mapped.Set(dst_t.Get());
                else{
                    Root reflected(api,api.type_get_object(api.class_get_type(actual)));void* args[]={reflected.Get()};
                    Root check(api,api.Call(get_component,src_t.Get(),args));if(check.Get()!=value.Get())continue;
                    mapped.Set(api.Call(get_component,dst_t.Get(),args));
                }
                if(!Alive(mapped.Get())||api.object_get_class(mapped.Get())!=actual)continue;
            }else mapped.Set(value.Get()); // external scene reference; never share mutable lists
            if(mapped.Get()!=prior.Get()){
                api.field_set_value_object(copy,f,mapped.Get());Object* after=nullptr;api.field_get_value(copy,f,&after);if(after!=mapped.Get())return false;
            }
        }return true;
    }
    bool BindCloneComponent(Object* root,Object* owner,const char* name){
        auto* f=api.class_get_field_from_name(api.object_get_class(owner),name);if(!f)return false;
        auto* type=api.class_from_type(api.field_get_type(f));if(!type||!Derives(type,"UnityEngine","Component"))return false;
        Root value(api,Ref(owner,name)),t(api,value.Get()?api.Call(transform,value.Get()):nullptr),rt(api,api.Call(go_transform,root));
        if(Alive(t.Get())&&ChildOf(t.Get(),rt.Get()))return true;
        Root selected(api);bool ambiguous=false;
        if(!Children(root,type,[&](Object* o){if(selected.Get()&&selected.Get()!=o)ambiguous=true;selected.Set(o);})||ambiguous||!selected.Get())return false;
        return SetReference(owner,name,selected.Get(),api.class_get_name(type),api.class_get_namespace(type));
    }
    bool BindSceneComponent(Object* owner,const char* name){
        if(Alive(Ref(owner,name)))return true;
        auto* f=api.class_get_field_from_name(api.object_get_class(owner),name);if(!f)return false;
        auto* type=api.class_from_type(api.field_get_type(f));if(!type||!Derives(type,"UnityEngine","Component"))return false;
        Root found(api);bool ambiguous=false;
        if(!Each(type,[&](Object* o){if(!InHierarchy(o))return;if(found.Get()&&found.Get()!=o)ambiguous=true;found.Set(o);})||ambiguous||!found.Get())return false;
        return SetReference(owner,name,found.Get(),api.class_get_name(type),api.class_get_namespace(type));
    }
    bool PrepareCloneBehavior(Object* ai,size_t kind){
        if(kind>2||!Alive(ai))return false;
        Root source(api),copy_root(api,api.Call(transform,ai)),root_go(api,api.Call(go,ai)),own_view(api,Ref(ai,"pv"));
        for(const auto& clone:clones)if(clone->controller.Get()==ai){
            own_view.Set(clone->view.Get());root_go.Set(clone->object.Get());copy_root.Set(api.Call(go_transform,root_go.Get()));break;
        }
        if(!Alive(copy_root.Get())||!Alive(root_go.Get())||!Alive(own_view.Get()))return false;
        if(!SetReference(ai,"pv",own_view.Get(),"PhotonView","Photon.Pun"))return false;
        for(const auto& actor:actors){
            if(actor->kind!=kind||actor->object.Get()==ai||!InHierarchy(actor->object.Get()))continue;
            Root v(api,Ref(actor->object.Get(),"pv"));
            if(std::any_of(clones.begin(),clones.end(),[&](const auto& c){return c->view.Get()==v.Get();}))continue;
            source.Set(actor->object.Get());break;
        }
        // Original actors are optional. A player may remove them before
        // cloning; serialized prefab references and this hierarchy are primary.
        Root original_root(api),original_eye(api);
        if(source.Get()){
            original_root.Set(api.Call(transform,source.Get()));Root view(api,Ref(source.Get(),"pv")),vr(api,view.Get()?api.Call(transform,view.Get()):nullptr);
            if(Alive(vr.Get())&&ChildOf(original_root.Get(),vr.Get()))original_root.Set(vr.Get());
            RepairCloneReferences(source.Get(),ai,original_root.Get(),copy_root.Get());
            original_eye.Set(kind<2?Ref(source.Get(),"enemyEye"):source.Get());
        }
        if(kind<2&&!BindCloneComponent(root_go.Get(),ai,"enemyEye"))return false;
        if(!BindCloneComponent(root_go.Get(),ai,specs[kind].animator))return false;
        Root eye(api,kind<2?Ref(ai,"enemyEye"):ai),animator(api,Ref(ai,specs[kind].animator));
        if(original_eye.Get())RepairCloneReferences(original_eye.Get(),eye.Get(),original_root.Get(),copy_root.Get());
        if(!SetReference(eye.Get(),"pv",own_view.Get(),"PhotonView","Photon.Pun"))return false;
        if(kind<2){
            if(!SetReference(eye.Get(),kind==0?"enemyAIGrannyMulti":"enemyAIGrandpaMulti",ai,specs[kind].klass,""))return false;
            Root t(api,api.Call(transform,eye.Get()));if(!SetReference(eye.Get(),"myTransform",t.Get(),"Transform"))return false;
            if(kind==0&&!BindSceneComponent(eye.Get(),"daysManager"))return false;
        }
        if(!BindCloneComponent(root_go.Get(),eye.Get(),"cam"))return false;
        for(const char* name:{"seeRange",kind==1?"grandpaUpdateParseRate":"grannyUpdateParseRate"}){
            auto* f=api.class_get_field_from_name(api.object_get_class(eye.Get()),name);if(!FieldType(api,f,"System","Single"))continue;
            float value=0,from=0;api.field_get_value(eye.Get(),f,&value);if(original_eye.Get())api.field_get_value(original_eye.Get(),f,&from);
            if((!std::isfinite(value)||value<=0)&&std::isfinite(from)&&from>0&&from<1000)api.field_set_value(eye.Get(),f,&from);
        }
        auto* mask=api.class_get_field_from_name(api.object_get_class(eye.Get()),"viewMask");
        if(original_eye.Get()&&FieldType(api,mask,"UnityEngine","LayerMask")&&api.class_value_size){
            auto* type=api.class_from_type(api.field_get_type(mask));uint32_t alignment=0;
            if(api.class_value_size(type,&alignment)==sizeof(int)&&alignment<=alignof(int)){
                int current=0,from=0;api.field_get_value(eye.Get(),mask,&current);api.field_get_value(original_eye.Get(),mask,&from);if(!current&&from)api.field_set_value(eye.Get(),mask,&from);
            }
        }
        for(Object* component:{eye.Get(),animator.Get()}){
            Root t(api,api.Call(transform,component));if(!Alive(t.Get())||!ChildOf(t.Get(),copy_root.Get()))return false;
            for(int depth=0;t.Get()!=copy_root.Get()&&depth<32;++depth){
                Root object(api,api.Call(go,t.Get()));if(!Alive(object.Get()))return false;
                if(!api.Value<bool>(go_active,object.Get(),false)&&!Set(active_set,object.Get(),true))return false;
                t.Set(api.Call(parent_get,t.Get()));if(!Alive(t.Get()))return false;
            }
            if(t.Get()!=copy_root.Get())return false;
        }
        if(!Set(enabled_set,eye.Get(),true)||!Set(enabled_set,animator.Get(),true))return false;
        float speed=api.Value<float>(anim_get,animator.Get(),-1);if(!std::isfinite(speed)||speed<0)return false;
        if(speed==0&&!Set(anim_set,animator.Get(),1.f))return false;
        if(anim_controller_get){Root controller(api,api.Call(anim_controller_get,animator.Get()));if(!Alive(controller.Get()))return false;}
        // These are the game's own per-instance target-list initializers. Do
        // not replay Start/OnEnable or raise a global event for every enemy.
        const auto* targets=api.FindMethod(api.object_get_class(ai),kind==0?"ekr":kind==1?"dwi":"ims",0);
        const auto* players=api.FindMethod(api.object_get_class(eye.Get()),kind<2?"GetPlayers":"ims",0);
        if(!targets||!players)return false;
        bool ok=false;api.Call(targets,ai,nullptr,&ok);if(!ok)return false;
        if(players!=targets)api.Call(players,eye.Get(),nullptr,&ok);
        return ok;
    }
    bool EnsureCloneLocomotion(CloneRecord& clone){
        auto* ai=clone.controller.Get();if(!Alive(ai)||clone.kind>=kEnemyCount)return false;
        Root root(api,api.Call(go_transform,clone.object.Get())),ai_root(api,api.Call(transform,ai));
        if(!Alive(root.Get())||!Alive(ai_root.Get())||!ChildOf(ai_root.Get(),root.Get()))return false;
        if(!Alive(clone.view.Get())||!api.Value<bool>(mine,clone.view.Get(),false))return false;
        const auto bind=[&](const char* field,Class* type,const char* name,const char* space){
            if(!field||!type)return false;
            Root value(api,Ref(ai,field));
            if(value.Get()){Root t(api,api.Call(transform,value.Get()));if(Alive(t.Get())&&ChildOf(t.Get(),root.Get()))return true;}
            Root chosen(api);bool ambiguous=false;
            if(!Children(clone.object.Get(),type,[&](Object* component){if(chosen.Get()&&chosen.Get()!=component)ambiguous=true;chosen.Set(component);})||ambiguous||!chosen.Get())return false;
            return SetReference(ai,field,chosen.Get(),name,space);
        };
        if(!bind(specs[clone.kind].nav,nav_class,"NavMeshAgent","UnityEngine.AI")||!bind(specs[clone.kind].animator,animator_class,"Animator","UnityEngine"))return false;
        Root nav(api,Ref(ai,specs[clone.kind].nav)),animator(api,Ref(ai,specs[clone.kind].animator));
        if(!Set(enabled_set,nav.Get(),true))return false;
        if(nav_update_pos&&!Set(nav_update_pos,nav.Get(),true))return false;
        if(nav_update_rot&&!Set(nav_update_rot,nav.Get(),true))return false;
        Set(enabled_set,animator.Get(),true);
        const float speed=api.Value<float>(anim_get,animator.Get(),-1);
        if(speed==0)Set(anim_set,animator.Get(),1.f);
        return true;
    }
    void ProbeClone(CloneRecord& clone,double now){
        if(clone.kind>=kEnemyCount||now<clone.probe_next||clone.probes>=9)return;
        clone.probe_next=now+1;++clone.probes;
        Root t(api,api.Call(go_transform,clone.object.Get())),nav(api,Ref(clone.controller.Get(),specs[clone.kind].nav));
        Vec3 p{};Quat q{};const bool position_ok=Pose(t.Get(),p,q);
        bool moved=false;if(position_ok&&clone.observation_ready){const float dx=p.x-clone.observed_position.x,dz=p.z-clone.observed_position.z;moved=dx*dx+dz*dz>.0025f;}
        if(position_ok&&!clone.observation_ready){clone.observed_position=p;clone.observation_ready=true;}
        const bool nav_on=Alive(nav.Get())&&api.Value<bool>(enabled_get,nav.Get(),false);
        const float speed=nav_on?api.Value<float>(nav_get,nav.Get(),-1):-1;
        const bool path=nav_on&&api.Value<bool>(nav_has_path,nav.Get(),false),pending=nav_on&&api.Value<bool>(nav_pending,nav.Get(),false);
        __android_log_print(ANDROID_LOG_INFO,"multiplayermod","clone probe kind=%zu master=%d nav=%d speed=%.2f path=%d pending=%d route=%d behavior=%d moved=%d frames=%u senses=%u target=%d",clone.kind,api.Value<bool>(master,nullptr,false),nav_on,double(speed),path,pending,clone.route_initialized,clone.behavior_ready,moved,clone.completed_frames,clone.senses,clone.has_target);
        if(clone.probes==9&&!moved&&!settings[clone.kind].paused){
            snap.status=std::string(gameplay_bridge::kNames[clone.kind])+u8" klonu yürüdüğü doğrulanamadı. Nav: "+(nav_on?"on":"off")+u8", hız: "+std::to_string(speed)+u8", rota: "+(path?"var":pending?"bekliyor":"yok")+u8". Ayrıntı clone probe kaydında.";
        }
    }
    bool SenseClone(CloneRecord& clone,double now){
        // Poll the game's actual per-instance perception. A populated player
        // list alone never runs the sight test or enters native chase/attack.
        if(clone.kind>2||settings[clone.kind].paused||!WorldMatches()||!snap.authority||!api.Value<bool>(master,nullptr,false)||!Alive(clone.view.Get())||!api.Value<bool>(mine,clone.view.Get(),false)||!InHierarchy(clone.controller.Get()))return false;
        const auto blocked=[&](const char* reason){if(std::strcmp(clone.sense_issue,reason)){clone.sense_issue=reason;__android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG clone sense-blocked kind=%zu reason=%s",clone.kind,reason);}return false;};
        auto* ai=clone.controller.Get();
        for(const char* name:{"gotHitByWeapon","eatingPlayer","playerGetCaught","attackingPlayer","hitByArrow","hitByGun","hitByCar","hitByPepperStart","bastuKilled","freeze"})if(Flag(ai,name))return false;
        if(now<clone.sense_next)return false;
        clone.sense_next=now+.25;
        if(!clone.behavior_ready&&now>=clone.bind_next){
            if(clone.bind_tries<1000000)++clone.bind_tries;
            clone.bind_next=now+(clone.bind_tries<12?2:10);clone.behavior_ready=PrepareCloneBehavior(ai,clone.kind);
            __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG clone bind kind=%zu ready=%d attempt=%u",clone.kind,clone.behavior_ready,clone.bind_tries);
        }
        if(Ref(ai,"pv")!=clone.view.Get())return blocked("controller-view");
        Root sensor(api,clone.kind<2?Ref(ai,"enemyEye"):ai);
        Root root(api,api.Call(go_transform,clone.object.Get())),sensor_root(api,sensor.Get()?api.Call(transform,sensor.Get()):nullptr);
        if(!Alive(sensor.Get())||!Alive(root.Get())||!Alive(sensor_root.Get())||!ChildOf(sensor_root.Get(),root.Get()))return blocked("sensor-hierarchy");
        if(clone.kind<2){
            if(Ref(sensor.Get(),clone.kind==0?"enemyAIGrannyMulti":"enemyAIGrandpaMulti")!=ai)return blocked("sensor-controller");
            if(Ref(sensor.Get(),"pv")!=clone.view.Get())return blocked("sensor-view");
            for(const char* name:{"cam","myTransform"})if(!Alive(Ref(sensor.Get(),name)))return blocked(name);
            if(clone.kind==0&&!Alive(Ref(sensor.Get(),"daysManager")))return blocked("daysManager");
            if(PositiveField(sensor.Get(),"seeRange")<=0)return blocked("seeRange");
        }else if(!Alive(Ref(ai,"cam"))||!Alive(Ref(ai,"agent")))return blocked("mom-camera-agent");
        Root camera(api,Ref(sensor.Get(),"cam")),camera_t(api,api.Call(transform,camera.Get()));
        if(!Alive(camera_t.Get())||!ChildOf(camera_t.Get(),root.Get()))return blocked("camera-hierarchy");
        const auto* refresh=api.FindMethod(api.object_get_class(sensor.Get()),clone.kind<2?"GetPlayers":"ims",0);
        const auto* check=api.FindMethod(api.object_get_class(sensor.Get()),clone.kind<2?"CheckForPlayersInView":"inc",0);
        if(!refresh||!check)return blocked("sight-method");
        bool ok=false;
        if(now>=clone.players_next){api.Call(refresh,sensor.Get(),nullptr,&ok);if(!ok){++clone.sense_errors;return blocked("native-exception");}clone.players_next=now+2;}
        if(clone.kind<2&&!Ref(sensor.Get(),"seeTargets"))return blocked("player-list");
        api.Call(check,sensor.Get(),nullptr,&ok);
        if(!ok){++clone.sense_errors;return blocked("native-exception");}
        clone.sense_issue="ready";++clone.senses;
        const bool target=clone.kind<2?(Ref(sensor.Get(),clone.kind==0?"pka":"pdz")&&Flag(ai,"seePlayer")):(Ref(ai,"rnx")&&Flag(ai,"isPlayerInView"));
        if(target){clone.last_seen=now;clone.investigating=false;}
        else if(clone.has_target&&now-clone.last_seen>=2){
            if(ReturnClonePatrol(clone)){clone.has_target=false;__android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG clone lost-sight kind=%zu patrol=1",clone.kind);}
        }
        // A brief occlusion keeps native pursuit; a sustained loss releases
        // this clone's target through the game's own per-instance cleanup.
        const bool chasing=target||clone.has_target;
        if(chasing!=clone.has_target){
            clone.has_target=chasing;
            __android_log_print(ANDROID_LOG_INFO,"multiplayermod","clone perception kind=%zu target=%d native_checks=%u",clone.kind,target,clone.senses);
            if(target)snap.status=std::string(gameplay_bridge::kNames[clone.kind])+u8" klonu görüş kontrolünde oyuncu hedefi edindi; takip/saldırıyı oyunun AI'si yürütür.";
        }
        return true;
    }
    bool CloneControlled(CloneRecord& clone){
        if(clone.kind>2||settings[clone.kind].paused||!WorldMatches()||!InHierarchy(clone.controller.Get())||!Alive(clone.view.Get())||!api.Value<bool>(mine,clone.view.Get(),false))return false;
        for(const char* n:{"gotHitByWeapon","eatingPlayer","playerGetCaught","attackingPlayer","grannyGonnaSmack","hitByArrow","hitByGun","hitByCar","hitByPepper","hitByPepperStart","bastuKilled","freeze"})if(Flag(clone.controller.Get(),n))return false;
        return true;
    }
    bool ReturnClonePatrol(CloneRecord& clone){
        auto* ai=clone.controller.Get();Root sensor(api,clone.kind<2?Ref(ai,"enemyEye"):ai);
        if(!Alive(sensor.Get()))return false;
        const auto* clear=api.FindMethod(api.object_get_class(sensor.Get()),clone.kind==0?"eli":clone.kind==1?"dwz":"inb",0);
        const auto* reset=api.FindMethod(api.object_get_class(ai),clone.kind==0?"eim":clone.kind==1?"duo":"inf",0);
        const auto* route=clone.kind<2?api.FindMethod(api.object_get_class(ai),clone.kind==0?"ein":"dup",0):nullptr;
        if(!clear||!reset||(clone.kind<2&&!route))return false;
        bool ok=false;api.Call(clear,sensor.Get(),nullptr,&ok);if(!ok)return false;
        api.Call(reset,ai,nullptr,&ok);if(!ok)return false;
        if(clone.kind<2){BoolField(ai,"seePlayer",false);SetReference(ai,"targetTransform",nullptr,"Transform");api.Call(route,ai,nullptr,&ok);}
        clone.route_next=0;return ok;
    }
    void CaptureNoise(Object* source,double now,float radius=20){
        if(!snap.ready||!WorldMatches()||!InHierarchy(source))return;
        Root t(api,api.Call(transform,source));Vec3 p{};Quat q{};if(!Pose(t.Get(),p,q))return;
        const float dx=p.x-noise_position.x,dz=p.z-noise_position.z;
        if(noise_serial&&now-noise_time<.4&&dx*dx+dz*dz<1&&radius<=noise_radius)return;
        noise_position=p;noise_time=now;noise_radius=radius;++noise_serial;
        __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG clone sound-event serial=%u at=%.2f,%.2f,%.2f",noise_serial,double(p.x),double(p.y),double(p.z));
    }
    void CaptureFootstep(Object* source,double now){
        if(now<footsteps_next||!snap.ready||!WorldMatches()||!InHierarchy(source)||!Alive(Ref(source,"pv")))return;
        footsteps_next=now+.5;CaptureNoise(source,now,8);
    }
    void ScanNoise(double now){
        if(now<noise_next||!snap.ready)return;
        noise_next=now+.2;
        std::vector<std::unique_ptr<Root>> current;
        for(auto* c:noise_classes)Each(c,[&](Object* o){
            if(!InHierarchy(o)||current.size()>=128)return;
            const bool known=std::any_of(noise_markers.begin(),noise_markers.end(),[&](const auto& p){return p->Get()==o;});
            current.push_back(std::make_unique<Root>(api,o));
            if(known||!noise_baseline)return;
            Root t(api,api.Call(transform,o));Vec3 p{};Quat q{};if(!Pose(t.Get(),p,q))return;
            const float dx=p.x-noise_position.x,dz=p.z-noise_position.z;
            if(now-noise_time<.4&&dx*dx+dz*dz<1)return; // Granny/Grandpa markers for one sound
            noise_position=p;noise_time=now;noise_radius=20;++noise_serial;
            __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG clone noise serial=%u at=%.2f,%.2f,%.2f",noise_serial,double(p.x),double(p.y),double(p.z));
        });
        noise_markers=std::move(current);noise_baseline=true;
    }
    void DriveCloneInvestigation(CloneRecord& clone,double now){
        if(!clone.investigating||clone.has_target||!CloneControlled(clone)||!snap.authority||!api.Value<bool>(master,nullptr,false))return;
        auto* ai=clone.controller.Get();
        if(Flag(ai,"seePlayer")||Flag(ai,"isPlayerInView")){clone.investigating=false;return;}
        Root t(api,api.Call(transform,ai)),nav(api,Ref(ai,specs[clone.kind].nav));Vec3 p{};Quat q{};
        if(!Alive(nav.Get())||!Pose(t.Get(),p,q))return;
        const float dx=p.x-clone.noise_position.x,dz=p.z-clone.noise_position.z;
        if(dx*dx+dz*dz<1&&clone.arrived_at==0)clone.arrived_at=now;
        if(now>=clone.investigate_until||(clone.arrived_at&&now-clone.arrived_at>=2)){
            if(ReturnClonePatrol(clone)){clone.investigating=false;__android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG clone investigate-done kind=%zu",clone.kind);}return;
        }
        // No shared waypoint or target transform is edited. Reapply only to
        // this clone after native FixedUpdate, while sight/attacks take priority.
        Vec3 destination=clone.arrived_at?p:clone.noise_position;void* args[]={&destination};api.Call(nav_destination,nav.Get(),args);
    }
    bool OpenCloneDoor(CloneRecord& clone,Object* door,double now){
        if(!door||api.object_get_class(door)!=door_class||!door_open||!CloneControlled(clone))return false;
        for(const char* n:{"doorOpen","doorMoving","doorLocked"}){
            auto* f=api.class_get_field_from_name(door_class,n);if(!FieldType(api,f,"System","Boolean")||ReadBool(door,f))return false;
        }
        for(auto& d:doors)if(d->object.Get()==door&&now<d->next)return false;
        bool ok=false;api.Call(door_open,door,nullptr,&ok);if(!ok)return false;
        doors.erase(std::remove_if(doors.begin(),doors.end(),[&](const auto& d){return !Alive(d->object.Get())||d->object.Get()==door;}),doors.end());
        if(doors.size()>=64)doors.erase(doors.begin());
        doors.push_back(std::make_unique<DoorCooldown>(api,door,now+2));
        __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG clone door kind=%zu native_open=1",clone.kind);return true;
    }
    void CloneDoorAhead(CloneRecord& clone,double now){
        if(now<clone.door_next||!door_line||!hit_collider||!hit_size||!get_component||!door_class||!CloneControlled(clone))return;
        clone.door_next=now+.25;
        Root t(api,api.Call(transform,clone.controller.Get()));Vec3 p{},end{};Quat q{};if(!Pose(t.Get(),p,q))return;
        Root target(api,Ref(clone.controller.Get(),"target"));Quat ignore{};
        bool aim=false;
        if(clone.investigating&&!clone.has_target){end=clone.noise_position;aim=true;}
        else aim=Alive(target.Get())&&Pose(target.Get(),end,ignore);
        float dx=aim?end.x-p.x:2*(q.x*q.z+q.w*q.y),dz=aim?end.z-p.z:1-2*(q.x*q.x+q.y*q.y);
        const float distance=std::sqrt(dx*dx+dz*dz);if(distance<.05f)return;
        p.y+=.8f;end={p.x+dx/distance*1.8f,p.y,p.z+dz/distance*1.8f};
        alignas(std::max_align_t) std::array<unsigned char,512> hit{};int mask=~0,ignore_triggers=1;
        void* args[]={&p,&end,hit.data(),&mask,&ignore_triggers};bool ok=false;Root found(api,api.Call(door_line,nullptr,args,&ok));
        if(!ok||!found.Get()||!*static_cast<bool*>(api.object_unbox(found.Get())))return;
        Root collider(api,api.Call(hit_collider,reinterpret_cast<Object*>(hit.data()),nullptr,&ok));if(!ok||!Alive(collider.Get()))return;
        Root cursor(api,api.Call(transform,collider.Get())),type(api,api.type_get_object(api.class_get_type(door_class)));
        if(!type.Get())return;
        for(int i=0;i<8&&Alive(cursor.Get());++i){
            void* get[]={type.Get()};Root door(api,api.Call(get_component,cursor.Get(),get,&ok));if(!ok)return;
            if(Alive(door.Get())){OpenCloneDoor(clone,door.Get(),now);return;}
            cursor.Set(api.Call(parent_get,cursor.Get()));
        }
    }
    void RecoverCloneMovement(CloneRecord& clone,double now){
        if(now<clone.health_next)return;
        clone.health_next=now+1;
        if(clone.kind>2||!snap.authority||!api.Value<bool>(master,nullptr,false)||!WorldMatches()||settings[clone.kind].paused||!InHierarchy(clone.controller.Get())||!Alive(clone.view.Get())||!api.Value<bool>(mine,clone.view.Get(),false)){clone.health_observed=false;clone.attack_since=0;return;}
        auto* ai=clone.controller.Get();
        for(const char* n:{"gotHitByWeapon","hitByArrow","hitByGun","hitByCar","hitByPepper","hitByPepperStart","bastuKilled","freeze"})if(Flag(ai,n)){clone.health_observed=false;clone.attack_since=0;return;}
        const bool attacking=Flag(ai,"eatingPlayer")||Flag(ai,"playerGetCaught")||Flag(ai,"attackingPlayer")||Flag(ai,"grannyGonnaSmack");
        if(attacking){
            if(!clone.attack_since)clone.attack_since=now;
            if(now-clone.attack_since<15)return;
        }else clone.attack_since=0;
        Root t(api,api.Call(transform,ai)),nav(api,Ref(ai,specs[clone.kind].nav));Vec3 p{};Quat q{};
        if(!Alive(nav.Get())||!Pose(t.Get(),p,q))return;
        if(!clone.health_observed){clone.health_position=p;clone.still_since=now;clone.health_observed=true;if(!attacking)return;}
        const float dx=p.x-clone.health_position.x,dz=p.z-clone.health_position.z;
        if(!attacking&&dx*dx+dz*dz>.04f){clone.health_position=p;clone.still_since=now;return;}
        if(!attacking&&now-clone.still_since<12)return;
        // No teleport or revival. Native target/animation cleanup chooses a
        // new existing patrol point; perception may immediately reacquire prey.
        if(!ReturnClonePatrol(clone))return;
        if(clone.kind<2){
            for(const char* n:{"playerGetCaught","attackingPlayer","startAttack","grannyGonnaSmack","inTrigger","aboutKillingCollider","startWalk","stopWalk","waypointStop","waypointWaitTime","timerOnOff"})BoolField(ai,n,false);
            BoolField(ai,"waypointStart",true);BoolField(ai,"GrannyMoving",true);
        }else BoolField(ai,"eatingPlayer",false);
        if(!api.Value<bool>(enabled_get,nav.Get(),false))Set(enabled_set,nav.Get(),true);
        float speed=PositiveField(ai,clone.kind<2?"grannysVarSpeed":"wanderSpeed");if(speed>0)Set(nav_set,nav.Get(),speed);
        clone.has_target=clone.investigating=false;clone.attack_since=0;clone.still_since=now;clone.health_position=p;clone.sense_next=0;++clone.recoveries;
        __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG clone recover kind=%zu reason=%s count=%u",clone.kind,attacking?"attack-timeout":"no-motion",clone.recoveries);
    }
    void UpdateCloneEnvironment(CloneRecord& clone,double now){
        if(!CloneControlled(clone))return;
        CloneDoorAhead(clone,now);
        if(!snap.authority||!api.Value<bool>(master,nullptr,false))return;
        if(clone.heard!=noise_serial){
            clone.heard=noise_serial;
            Root t(api,api.Call(transform,clone.controller.Get()));Vec3 p{};Quat q{};
            if(!clone.has_target&&noise_serial&&now-noise_time<4&&Pose(t.Get(),p,q)){
                const float dx=p.x-noise_position.x,dy=p.y-noise_position.y,dz=p.z-noise_position.z;
                if(dx*dx+dy*dy+dz*dz<=noise_radius*noise_radius){clone.noise_position=noise_position;clone.investigating=true;clone.investigate_until=now+10;clone.arrived_at=0;
                    __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG clone investigate kind=%zu sound=%u",clone.kind,noise_serial);}
            }
        }
        DriveCloneInvestigation(clone,now);
    }
    bool BindCloneRoute(Object* ai,size_t kind){
        if(kind>=2)return true;
        Root source(api),route(api);
        for(const auto& actor:actors){
            if(actor->kind!=kind||actor->object.Get()==ai||!InHierarchy(actor->object.Get()))continue;
            Root pv(api,Ref(actor->object.Get(),specs[kind].pv));
            if(std::any_of(clones.begin(),clones.end(),[&](const auto& c){return c->view.Get()==pv.Get();}))continue;
            source.Set(actor->object.Get());break;
        }
        if(source.Get()){
            Root manager(api,Ref(source.Get(),"gameManager"));if(Alive(manager.Get()))SetReference(ai,"gameManager",manager.Get(),"GameObject");
            for(int i=1;i<=16;++i){const auto name="nav"+std::to_string(i);Root point(api,Ref(source.Get(),name.c_str()));
                if(Alive(point.Get())&&SetReference(ai,name.c_str(),point.Get(),"Transform")&&!route.Get())route.Set(point.Get());
            }
            for(const char* name:{"bedtargetTemp1","bedtargetTemp2","bedtargetTemp3","coffintargetTemp4","coffintargetTempBY","cartargetTemp","PlayerCoffinPos","PlayerCoffinBYPos","PlayerCarPos","noisePointRat"}){
                Root point(api,Ref(source.Get(),name));if(Alive(point.Get()))SetReference(ai,name,point.Get(),"Transform");
            }
            Root door(api,Ref(source.Get(),"bastuDoor")),obstacle(api,Ref(source.Get(),"bastuDoorCarv"));
            if(Alive(door.Get()))SetReference(ai,"bastuDoor",door.Get(),"GameObject");
            if(Alive(obstacle.Get()))SetReference(ai,"bastuDoorCarv",obstacle.Get(),"NavMeshObstacle","UnityEngine.AI");
            for(const char* name:{"grannysVarSpeed","grannysAnimSpeed"}){
                auto* field=api.class_get_field_from_name(api.object_get_class(ai),name);if(!FieldType(api,field,"System","Single"))continue;
                float value=0,template_value=0;api.field_get_value(ai,field,&value);api.field_get_value(source.Get(),field,&template_value);
                if((!std::isfinite(value)||value<=0)&&std::isfinite(template_value)&&template_value>0&&template_value<100)api.field_set_value(ai,field,&template_value);
            }
        }
        if(!route.Get())route.Set(Ref(ai,"target"));
        if(!Alive(route.Get())||!SetReference(ai,"target",route.Get(),"Transform"))return false;
        // A late clone can miss the level's initial walk event. Only this
        // fresh clone's route state is initialized; no main manager is replaced.
        BoolField(ai,"waypointStop",false);BoolField(ai,"waypointStart",true);
        BoolField(ai,"waypointWaitTime",false);BoolField(ai,"timerOnOff",false);
        // startWalk is a one-shot animation latch, not a movement-enable bit.
        // Leave it false so the original FixedUpdate performs its walk entry.
        BoolField(ai,"startWalk",false);BoolField(ai,"stopWalk",false);BoolField(ai,"GrannyMoving",true);
        Root nav(api,Ref(ai,specs[kind].nav));Vec3 destination{};Quat q{};
        if(!Alive(nav.Get())||!api.Value<bool>(enabled_get,nav.Get(),false)||!Pose(route.Get(),destination,q)||!nav_destination)return false;
        auto* speed=api.class_get_field_from_name(api.object_get_class(ai),"grannysVarSpeed");
        if(FieldType(api,speed,"System","Single")){float value=0;api.field_get_value(ai,speed,&value);if(std::isfinite(value)&&value>0&&value<100)Set(nav_set,nav.Get(),value);}
        void* args[]={&destination};bool ok=false;Root result(api,api.Call(nav_destination,nav.Get(),args,&ok));
        if(!ok||!result.Get()||!*static_cast<bool*>(api.object_unbox(result.Get())))return false;
        return !nav_has_path||!nav_pending||api.Value<bool>(nav_has_path,nav.Get(),false)||api.Value<bool>(nav_pending,nav.Get(),false);
    }
    template<class T> bool AnimatorParameter(Object* ai,Object* animator,const char* name,const Method* method,T value){
        if(!method||!api.field_static_get_value||!api.field_get_flags)return false;
        auto* field=api.class_get_field_from_name(api.object_get_class(ai),name);
        if(!FieldType(api,field,"System","Int32")||!(api.field_get_flags(field)&0x10))return false;
        int id=0;api.field_static_get_value(field,&id);if(!id)return false;
        void* args[]={&id,&value};bool ok=false;api.Call(method,animator,args,&ok);return ok;
    }
    void AnimateClones(){
        if(!WorldMatches()||!nav_velocity)return;
        const bool local_master=snap.authority&&api.Value<bool>(master,nullptr,false);
        for(const auto& clone:clones){
            if((!local_master&&!clone->client_patrol)||!clone->route_initialized||clone->kind>1||!Alive(clone->view.Get())||!api.Value<bool>(mine,clone->view.Get(),false))continue;
            auto* ai=clone->controller.Get();if(!Alive(ai)||settings[clone->kind].paused)continue;
            // A late clone can retain a stale startWalk animation latch even
            // while its agent moves. Repair locomotion only, never overwrite
            // attacks, door interactions, searches or genuine enemy death.
            bool special=false;
            for(const char* name:{"playerGetCaught","attackingPlayer","startAttack","grannyGonnaSmack","grannySeeDoor","grannySeeLockedDoor","grannyLookUnderBed","grannyStandBesideCar","GrannySearching","droppingBeartrap","hitByArrow","hitByGun","hitByCar","hitByPepper","hitByPepperStart","bastuKilled","freeze"})special=special||Flag(ai,name);
            if(special)continue;
            Root nav(api,Ref(ai,specs[clone->kind].nav)),animator(api,Ref(ai,specs[clone->kind].animator));
            if(!Alive(nav.Get())||!Alive(animator.Get())||!api.Value<bool>(enabled_get,nav.Get(),false))continue;
            const auto velocity=api.Value<Vec3>(nav_velocity,nav.Get());if(!Finite(velocity))continue;
            const bool walking=velocity.x*velocity.x+velocity.z*velocity.z>.0025f;
            AnimatorParameter(ai,animator.Get(),clone->kind==0?"pjr":"pdp",anim_bool,!walking);
            AnimatorParameter(ai,animator.Get(),clone->kind==0?"pjs":"pdq",anim_bool,walking);
            float animation_speed=PositiveField(ai,"grannysAnimSpeed");if(animation_speed<=0)animation_speed=1;
            AnimatorParameter(ai,animator.Get(),clone->kind==0?"pjt":"pdr",anim_float,animation_speed);
        }
    }
    void StopClientPatrol(CloneRecord& clone){
        if(clone.client_nav_enabled&&!api.Value<bool>(master,nullptr,false)&&Alive(clone.controller.Get())){
            Root nav(api,Ref(clone.controller.Get(),specs[clone.kind].nav));if(Alive(nav.Get()))Set(enabled_set,nav.Get(),false);
        }
        clone.client_nav_enabled=false;clone.client_patrol=false;
    }
    bool DriveClientClone(CloneRecord& clone,double now){
        // The native FixedUpdate returns early on non-master peers. Drive
        // only our newly created owned clone's patrol using its real agent.
        // No host identity/ownership policy or original scene AI is changed.
        if(clone.kind>2||!Alive(clone.view.Get())||!api.Value<bool>(mine,clone.view.Get(),false)||!WorldMatches())return false;
        auto* ai=clone.controller.Get();if(!InHierarchy(ai))return false;
        if(settings[clone.kind].paused)return true;
        for(const char* name:{"gotHitByWeapon","eatingPlayer","playerGetCaught","attackingPlayer","grannyGonnaSmack","hitByArrow","hitByGun","hitByCar","hitByPepperStart","bastuKilled","freeze"})if(Flag(ai,name))return true;
        if(clone.kind<2&&!clone.behavior_ready&&clone.attempts<3){++clone.attempts;clone.behavior_ready=PrepareCloneBehavior(ai,clone.kind);}
        if(nav_class&&!EnsureCloneLocomotion(clone))return false;
        Root nav(api,Ref(ai,specs[clone.kind].nav)),animator(api,Ref(ai,specs[clone.kind].animator));
        if(!Alive(nav.Get())||!Alive(animator.Get())||!nav_destination)return false;
        if(!api.Value<bool>(enabled_get,nav.Get(),false)){
            if(!Set(enabled_set,nav.Get(),true))return false;
            clone.client_nav_enabled=true;
        }
        if(clone.kind<2&&!clone.route_initialized){
            if(!BindCloneRoute(ai,clone.kind))return false;
            clone.route_initialized=true;
        }
        const char* speed_name=clone.kind==2?"wanderSpeed":"grannysVarSpeed";
        float speed=PositiveField(ai,speed_name);
        for(const auto& actor:actors)if(actor->object.Get()==ai)for(const auto& field:actor->speed_fields)
            if(field.changed&&field.field==api.class_get_field_from_name(api.object_get_class(ai),speed_name))speed=field.base;
        if(speed<=0)return false;
        // Apply() applies the multiplier once after this base-speed write.
        Set(nav_set,nav.Get(),speed);
        if(clone.kind<2){
            if(now>=clone.route_next){
                Root t(api,api.Call(transform,ai));Vec3 position{};Quat rotation{};
                if(!Pose(t.Get(),position,rotation))return false;
                bool accepted=false;
                for(size_t i=0;i<16;++i){
                    const std::string name="nav"+std::to_string(clone.waypoint%16+1);
                    Root point(api,Ref(ai,name.c_str()));Vec3 destination{};
                    if(!Alive(point.Get())||!Pose(point.Get(),destination,rotation)){++clone.waypoint;continue;}
                    const float dx=destination.x-position.x,dz=destination.z-position.z;
                    if(dx*dx+dz*dz<1){++clone.waypoint;continue;}
                    void* args[]={&destination};bool ok=false;Root result(api,api.Call(nav_destination,nav.Get(),args,&ok));
                    if(ok&&result.Get()&&*static_cast<bool*>(api.object_unbox(result.Get()))){accepted=true;break;}
                    ++clone.waypoint;
                }
                if(!accepted)return false;
                clone.route_next=now+.5;
            }
            if(!api.Value<bool>(enabled_get,animator.Get(),false))Set(enabled_set,animator.Get(),true);
            return true;
        }
        if(now>=clone.route_next){
            Root points(api,Ref(ai,"wayPoints"));if(!points.Get())return false;
            auto* c=api.object_get_class(points.Get());const auto* count=api.FindMethod(c,"get_Count",0);const auto* get=api.FindMethod(c,"get_Item",1);
            int n=api.Value<int>(count,points.Get(),-1);
            if(n==0&&!clone.client_routes_prepared){
                // Native imw only builds this instance's waypoint list and
                // difficulty speeds. Start's non-master branch skips it.
                const auto* initialize=api.FindMethod(api.object_get_class(ai),"imw",0);if(!initialize)return false;
                bool ok=false;api.Call(initialize,ai,nullptr,&ok);if(!ok)return false;
                clone.client_routes_prepared=true;n=api.Value<int>(count,points.Get(),-1);
            }
            if(n<=0||n>128)return false;
            Root t(api,api.Call(transform,ai));Vec3 position{};Quat rotation{};if(!Pose(t.Get(),position,rotation))return false;
            // Keep the same waypoint until reached; a periodic retry also
            // recovers a rejected/unavailable path without tight loops.
            int index=int(clone.waypoint%size_t(n));void* args[]={&index};Root point(api,api.Call(get,points.Get(),args));Vec3 destination{};
            if(!Alive(point.Get())||!Pose(point.Get(),destination,rotation))return false;
            const float dx=destination.x-position.x,dz=destination.z-position.z;
            if(dx*dx+dz*dz<1){++clone.waypoint;index=int(clone.waypoint%size_t(n));point.Set(api.Call(get,points.Get(),args));if(!Alive(point.Get())||!Pose(point.Get(),destination,rotation))return false;}
            void* target[]={&destination};bool ok=false;Root accepted(api,api.Call(nav_destination,nav.Get(),target,&ok));
            if(!ok||!accepted.Get()||!*static_cast<bool*>(api.object_unbox(accepted.Get())))return false;
            clone.route_next=now+.5;
        }
        if(!api.Value<bool>(enabled_get,animator.Get(),false))Set(enabled_set,animator.Get(),true);
        const auto* walk=api.FindMethod(api.object_get_class(ai),"imz",0);if(!walk)return false;
        bool ok=false;api.Call(walk,ai,nullptr,&ok);return ok;
    }
    void UpdateClones(){
        const double now=gameplay_bridge::Now();
        for(auto& clone:clones){
            if(now-clone->born>.5){SenseClone(*clone,now);RecoverCloneMovement(*clone,now);UpdateCloneEnvironment(*clone,now);}
            if(Alive(clone->controller.Get())&&Alive(clone->object.Get())&&now-clone->born>.25)ProbeClone(*clone,now);
            if(clone->client_patrol){
                if(api.Value<bool>(master,nullptr,false)){
                    clone->client_nav_enabled=false;clone->client_patrol=false;clone->finished=false;clone->born=now;clone->attempts=0;clone->route_initialized=false;
                }else if(now>=clone->next){
                    clone->next=now+.2;
                    if(!DriveClientClone(*clone,now)){
                        // Stop our driver as soon as ownership is lost. The
                        // new controlling peer owns its navigation from here.
                        StopClientPatrol(*clone);snap.status=u8"Klonun yerel devriyesi durdu; sahiplik veya rota bağlantısı değişti.";
                    }
                }
                if(clone->finished)continue;
            }
            if(clone->finished||now<clone->next||now-clone->born<.25)continue;
            clone->next=now+.5;
            if(!Alive(clone->object.Get())||!Alive(clone->view.Get())){clone->finished=true;continue;}
            if(!master_client||!actor_number||!owner_number){clone->finished=true;snap.status=u8"Klon görünür; AI kontrolünü doğrulayacak ağ bağlantısı eksik.";continue;}
            Root leader(api,api.Call(master_client));int leader_id=api.Value<int>(actor_number,leader.Get(),0);
            const int owner=api.Value<int>(owner_number,clone->view.Get(),-1);
            if(leader_id<=0||owner<0){if(now-clone->born>5)clone->finished=true;continue;}
            const bool local_master=api.Value<bool>(master,nullptr,false);
            if(owner==leader_id||owner==0){
                if(local_master){
                    bool ready=true,waiting_pause=false;unsigned controllers=0;
                    // Start has run by now. Only this newly created, locally
                    // controlled hierarchy may be activated/placed on its mesh.
                    for(size_t k=0;k<3;++k)Children(clone->object.Get(),classes[k],[&](Object* ai){
                        ++controllers;if(settings[k].paused){waiting_pause=true;return;}
                        clone->controller.Set(ai);
                        if(!clone->behavior_ready)clone->behavior_ready=PrepareCloneBehavior(ai,k);
                        if(nav_class&&!EnsureCloneLocomotion(*clone)){ready=false;return;}
                        Root nav(api,Ref(ai,specs[k].nav));if(!Alive(nav.Get())){ready=false;return;}
                        if(!api.Value<bool>(enabled_get,ai,false))Set(enabled_set,ai,true);
                        // Warp/isOnNavMesh were stripped from this player. SDK
                        // Instantiate already uses the sampled mesh position;
                        // rebind only this new agent after placing its root.
                        if(k<2&&!clone->placement_ready){
                            Root t(api,api.Call(go_transform,clone->object.Get()));
                            if(!Alive(t.Get())||!Set(enabled_set,nav.Get(),false)||!Set(pos_set,t.Get(),clone->position)||!Set(enabled_set,nav.Get(),true)){ready=false;return;}
                            clone->placement_ready=true;
                        }
                        if(!api.Value<bool>(enabled_get,nav.Get(),false))Set(enabled_set,nav.Get(),true);
                        if(!api.Value<bool>(enabled_get,nav.Get(),false)||!api.Value<bool>(enabled_get,ai,false)){ready=false;return;}
                        if(!clone->route_initialized&&!BindCloneRoute(ai,k))ready=false;
                    });
                    if(waiting_pause){clone->born=now;continue;}
                    if(!ready||!controllers){
                        if(++clone->attempts>=3||now-clone->born>5){clone->finished=true;snap.status=clone->behavior_ready?u8"Klonun AI/NavMesh başlangıcı doğrulanamadı; saldırı/hareket hazır sayılmadı.":u8"Klonun kendi görme/animasyon/sahne bağlantısı tamamlanamadı; yalnız hareket saldırı başarısı değildir.";}
                        continue;
                    }
                    if(!clone->route_initialized){
                        clone->route_initialized=true;
                        Root t(api,api.Call(go_transform,clone->object.Get()));Quat q{};
                        clone->observation_ready=Pose(t.Get(),clone->observed_position,q);
                        snap.status=u8"Klon rotası kabul edildi; gerçek konum hareketi doğrulanıyor.";continue;
                    }
                    Root t(api,api.Call(go_transform,clone->object.Get()));Vec3 current{};Quat q{};
                    if(clone->observation_ready&&Pose(t.Get(),current,q)){
                        const float dx=current.x-clone->observed_position.x,dz=current.z-clone->observed_position.z;
                        if(dx*dx+dz*dz>.0025f&&clone->completed_frames){clone->finished=true;snap.status=clone->behavior_ready?u8"Klonun konum değişimi ve özgün AI karesinin tamamlandığı doğrulandı; animasyon/görme bağlantıları hazır.":u8"Klonun yürüdüğü doğrulandı; göz/saldırı bağlantısı eksik, tam AI hazır sayılmadı.";continue;}
                    }
                    if(now-clone->born>8){clone->finished=true;snap.status=clone->completed_frames?u8"Klonun AI karesi çalışıyor ancak yürüdüğü doğrulanamadı.":u8"Klonun rotası var ancak özgün AI karesinin tamamlandığı doğrulanamadı; kapı/saldırı hazır sayılmadı.";}
                    continue;
                }
                clone->finished=true;snap.status=u8"Klonun kontrolü oda sahibiyle eşleşti; bu cihazdan uzaktaki AI hareketi doğrulanamaz.";continue;
            }
            if(!api.Value<bool>(mine,clone->view.Get(),false)){
                clone->finished=true;snap.status=u8"Klon görünür; kontrol başka oyuncuda. AI için oda sahibiyle eşleşme doğrulanamadı.";continue;
            }
            if(clone->kind<kEnemyCount&&settings[clone->kind].paused){clone->born=now;continue;}
            clone->client_patrol=DriveClientClone(*clone,now);
            if(clone->client_patrol){clone->finished=true;snap.status=u8"Sana ait klonun client devriyesi açıldı. Özgün host saldırı/kapı AI'si bu cihazda çalışmaz.";continue;}
            if(++clone->attempts<3&&now-clone->born<5)continue;
            StopClientPatrol(*clone);clone->finished=true;snap.status=u8"Klonun client rota/animasyon bağlantısı hazırlanamadı; devriye başlatılmadı.";

        }
        clones.erase(std::remove_if(clones.begin(),clones.end(),[&](const auto& clone){return !Alive(clone->object.Get());}),clones.end());
    }
    std::string VoteName(Object* p){
        Root text(api,api.Call(vote_nick,p));auto value=chat_bot_core::PrefixScalars(chat_bot_core::StripTags(api.Utf8(reinterpret_cast<String*>(text.Get()))),48);
        for(char& c:value)if(c=='\n'||c=='\r')c=' ';
        return value.empty()?"Player":value;
    }
    Object* VotePlayer(int actor,Object* expected=nullptr){
        Root array(api,api.Call(vote_players));const int n=array.Get()?api.Value<int>(length,array.Get(),-1):-1;if(n<0||n>128)return nullptr;
        for(int i=0;i<n;++i){void* args[]={&i};Root p(api,api.Call(item,array.Get(),args));
            if(p.Get()&&api.Value<int>(actor_number,p.Get(),0)==actor&&(!expected||p.Get()==expected))return p.Get();
        }return nullptr;
    }
    bool VoteRoomReady(){
        return snap.ready&&room_id&&reinterpret_cast<uintptr_t>(api.Call(current_room))==room_id;
    }
    bool VoteApi(){
        if(!vote_receive_ready||!vote_players||!vote_nick||!vote_local||!vote_time||!vote_send||!vote_input_field||!vote_allowed_field||!vote_text||!vote_set||!vote_focused)return false;
        if(!vote_decode){Root encoding(api,api.Call(vote_utf8));if(encoding.Get())vote_decode=ByteDecoder(api,api.object_get_class(encoding.Get()));}
        return vote_decode!=nullptr;
    }
    bool VoteNotice(const std::string& notice){
        if(!VoteRoomReady()||notice.empty()||notice.size()>2000)return false;
        Root owner(api);bool ambiguous=false;
        Each(vote_chat_class,[&](Object* p){if(!InHierarchy(p))return;if(owner.Get()&&owner.Get()!=p)ambiguous=true;owner.Set(p);});
        if(!owner.Get()||ambiguous)return false;
        bool allowed=false;api.field_get_value(owner.Get(),vote_allowed_field,&allowed);if(!chat_send_gate::Available(allowed))return false;
        Root input(api,Ref(owner.Get(),"inputField"));if(!Alive(input.Get()))return false;
        bool ok=false;Root focus(api,api.Call(vote_focused,input.Get(),nullptr,&ok));if(!ok||!focus.Get()||*static_cast<bool*>(api.object_unbox(focus.Get())))return false;
        Root old(api,api.Call(vote_text,input.Get(),nullptr,&ok));if(!ok||!old.Get()||!api.Utf8(reinterpret_cast<String*>(old.Get())).empty())return false;
        Root text(api,reinterpret_cast<Object*>(api.string_new(notice.c_str())));if(!text.Get())return false;
        void* args[]={text.Get()};api.Call(vote_set,input.Get(),args,&ok);if(!ok)return false;
        {chat_send_gate::AutomaticSend automatic;api.Call(vote_send,owner.Get(),nullptr,&ok);}const bool invoked=ok;
        Root after(api,api.Call(vote_text,input.Get(),nullptr,&ok));if(!ok||!after.Get())return false;
        const auto value=api.Utf8(reinterpret_cast<String*>(after.Get()));
        if(value==notice){void* reset[]={old.Get()};api.Call(vote_set,input.Get(),reset);return false;}
        return invoked&&value.empty()&&VoteRoomReady();
    }
    void EndVote(const std::string& status){vote_listening=false;ballot.Clear();snap.vote.active=snap.vote.counting=false;snap.vote.remaining=0;snap.vote.status=status;}
    void FinishVote(const std::string& status,double now){
        const auto name=ballot.name;EndVote(status);
        vote_result_notice=name+" | "+status;vote_result_deadline=now+15;vote_result_next=now;
    }
    std::string VoteCount()const{return "EVET: "+std::to_string(ballot.yes)+" | HAYIR: "+std::to_string(ballot.no);}
    void ScanVote(double now){
        snap.vote.ready=VoteRoomReady()&&VoteApi();
        if(now<vote_scan)return;
        vote_scan=now+1;snap.vote.players.clear();
        Root self(api,api.Call(vote_local));snap.vote.local=self.Get()?api.Value<int>(actor_number,self.Get(),0):0;
        Root array(api,api.Call(vote_players));const int n=array.Get()?api.Value<int>(length,array.Get(),-1):-1;
        if(n>=0&&n<=128)for(int i=0;i<n;++i){void* args[]={&i};Root p(api,api.Call(item,array.Get(),args));if(!p.Get())continue;const int id=api.Value<int>(actor_number,p.Get(),0);if(id>0)snap.vote.players.push_back({id,VoteName(p.Get())});}
        if(!ballot.Active()&&(!snap.vote.ready||snap.vote.status.empty()))snap.vote.status=!snap.vote.ready?u8"Oda ve sohbet bağlantısı bekleniyor.":u8"Şaka oylaması hazır; host veya client başlatabilir.";
    }
    void StartVote(int target,int seconds,double now){
        vote_scan=0;ScanVote(now);if(!snap.vote.ready){snap.status=snap.vote.status;return;}
        Root self(api,api.Call(vote_local)),victim(api,VotePlayer(target));const int host=self.Get()?api.Value<int>(actor_number,self.Get(),0):0;
        if(!victim.Get()||!ballot.Start(host,target,VoteName(victim.Get()),seconds,snap.vote.players,now)){snap.vote.status=u8"Oylama başlatılamadı: hedef ayrılmış, kendin veya başka oylama açık.";return;}
        vote_result_notice.clear();
        vote_notice_next=0;vote_sent_yes=vote_sent_no=-1;snap.vote.status=u8"Başlangıç duyurusu bekleniyor; sohbet taslağını boş bırakıp klavyeyi kapat.";
    }
    void IncomingVote(Object* sender,const std::string& line,int32_t sent){
        if(!VoteRoomReady()||ballot.phase!=room_vote::Phase::Voting||!sender)return;
        const int actor=api.Value<int>(actor_number,sender,0);Root current(api,VotePlayer(actor,sender));if(!current.Get())return;
        Root nick(api,api.Call(vote_nick,current.Get()));if(!nick.Get())return;
        const auto body=chat_bot_core::MatchSender(line,api.Utf8(reinterpret_cast<String*>(nick.Get())));if(!body)return;
        const auto command=chat_bot_core::Fold(chat_bot_core::Trim(*body));if(command!="/evet"&&command!="/hayir")return;
        ballot.eligible.insert(actor); // late joiners must still be current, authenticated room members
        if(ballot.Cast(actor,command=="/evet",gameplay_bridge::Now(),sent))snap.vote.status=u8"Oy alındı. Her oyuncunun yalnız ilk oyu sayılır.";
    }
    void UpdateVote(double now){
        ScanVote(now);
        using Phase=room_vote::Phase;
        if(!ballot.Active()){
            vote_listening=false;
            if(!vote_result_notice.empty()){
                if(!VoteRoomReady()||now>vote_result_deadline)vote_result_notice.clear();
                else if(now>=vote_result_next){vote_result_next=now+.75;if(VoteNotice(vote_result_notice))vote_result_notice.clear();}
            }return;
        }
        if(!snap.vote.ready||!VoteRoomReady()){EndVote(u8"Oda veya sohbet bağlantısı değişti; oylama iptal edildi.");return;}
        Root target(api,VotePlayer(ballot.target));
        if(!target.Get()&&ballot.phase!=Phase::PrankDelay){EndVote(u8"Hedef oyuncu ayrıldı; oylama sona erdi.");return;}
        Root self(api,api.Call(vote_local));if(!self.Get()||api.Value<int>(actor_number,self.Get(),0)!=ballot.host){EndVote(u8"Oylamayı başlatan oyuncu değişti; iptal edildi.");return;}
        snap.vote.active=true;snap.vote.target=ballot.target;snap.vote.yes=ballot.yes;snap.vote.no=ballot.no;snap.vote.remaining=ballot.Remaining(now);snap.vote.counting=ballot.phase==Phase::Voting;
        if(ballot.phase==Phase::Voting&&now>=ballot.deadline){ballot.phase=Phase::Result;ballot.deadline=now+15;vote_listening=false;vote_notice_next=now;}
        if(ballot.phase==Phase::Announce||ballot.phase==Phase::Result){
            if(now>ballot.deadline){EndVote(u8"Sohbet duyurusu iletilemedi; oyuncu atılmadı.");return;}
            if(now<vote_notice_next)return;
            vote_notice_next=now+.75;
            const bool start=ballot.phase==Phase::Announce;
            const auto notice=start?(ballot.name+u8" adlı oyuncu atılsın mı? /evet veya /hayır yazın. Süre: "+std::to_string(ballot.duration)+u8" sn. "+VoteCount()):
                (VoteCount()+(ballot.KickWins()?u8" | "+ballot.name+u8" adlı oyuncu atılıyor...":u8" | "+ballot.name+u8" odada kalıyor (hayır/eşitlik)."));
            if(!VoteNotice(notice))return;
            if(start){ballot.Open(now,api.Value<int32_t>(vote_time,nullptr,0));vote_listening=true;vote_sent_yes=ballot.yes;vote_sent_no=ballot.no;snap.vote.status=u8"Oylama açık: /evet veya /hayır.";}
            else if(ballot.KickWins()){ballot.phase=Phase::PrankDelay;ballot.deadline=now+1;vote_notice_next=now+1;snap.vote.status=u8"Şaka sonucu bekleniyor.";}
            else EndVote(u8"Oyuncu odada kaldı: hayır çoğunluğu veya eşitlik.");
            return;
        }
        if(ballot.phase==Phase::Voting&&now>=vote_notice_next&&(ballot.yes!=vote_sent_yes||ballot.no!=vote_sent_no)){
            vote_notice_next=now+.75;
            if(VoteNotice(ballot.name+" | "+VoteCount()+" | "+std::to_string(ballot.Remaining(now))+u8" sn kaldı.")){vote_sent_yes=ballot.yes;vote_sent_no=ballot.no;}
        }
        if(ballot.phase==Phase::PrankDelay&&now>=ballot.deadline){
            if(now>ballot.deadline+15){EndVote(u8"Şaka mesajı gönderilemedi; kimse çıkarılmadı.");return;}
            if(now<vote_notice_next)return;
            vote_notice_next=now+.75;
            const auto reveal=u8"Şaka şaka, "+ballot.name+u8" adlı kişiyi atmıyoruz ama adam olsun.";
            if(VoteNotice(reveal)){EndVote(reveal);__android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG prank complete: no disconnect requested");}
        }
    }
    void Capabilities(){
        for(size_t k=0;k<kEnemyCount;++k){snap.enemies[k].death_ready=false;snap.enemies[k].clone_ready=false;}
        const char* fields[]={"grannyPrefab","grandpaPrefab","spiderMomPrefab"};const char* deaths[]={"RPC_SyncShotShotgunGranny","RPC_SyncShotShotgunGrandpa"};
        for(size_t k=0;k<3;++k)Each(clone_managers[k],[&](Object* o){if(!InHierarchy(o))return;
            Root prefab(api,Ref(o,fields[k]));if(Alive(prefab.Get())&&spawn&&prefix_get&&path_combine&&nav_sample&&view_id)snap.enemies[k].clone_ready=true;
            if(k<2&&rpc&&api.array_new&&api.FindMethod(clone_managers[k],deaths[k],1)&&snap.enemies[k].count>0)snap.enemies[k].death_ready=true;
        });
        snap.enemies[2].death_ready=snap.enemies[2].count>0&&api.FindMethod(classes[2],"imv",2);
        snap.enemies[3].death_ready=snap.enemies[3].count>0&&view_id&&api.FindMethod(classes[3],"jek",2);
        snap.enemies[4].death_ready=snap.enemies[4].count>0;
    }
    void FindOwned(Class* type,Root& result){
        Root chosen(api),preferred(api),inventory_view(api,Ref(inventory.Get(),"pv"));bool ambiguous=false,preferred_ambiguous=false;
        Each(type,[&](Object* o){if(!InHierarchy(o)||!Owned(o,"pv"))return;
            if(chosen.Get()&&chosen.Get()!=o)ambiguous=true;
            chosen.Set(o);
            Root view(api,Ref(o,"pv"));if(inventory_view.Get()&&view.Get()==inventory_view.Get()){
                if(preferred.Get()&&preferred.Get()!=o)preferred_ambiguous=true;
                preferred.Set(o);
            }
        });result.Set(preferred_ambiguous?nullptr:preferred.Get()?preferred.Get():ambiguous?nullptr:chosen.Get());
    }
    void Tick(bool paused){
        if(busy)return;
        struct Guard{bool& busy;Guard(bool& b):busy(b){busy=true;}~Guard(){busy=false;}}guard(busy);
        if(paused){Clear(false);scene=-1;room_id=player_id=0;Diagnostics();gameplay_bridge::Publish(snap);return;}
        if(!initialized){initialized=true;valid=Resolve();}
        if(!valid){snap.status=u8"Oyun API bağlantısı bulunamadı; bu sürümde kontroller kullanılamıyor.";gameplay_bridge::Clear();Diagnostics();gameplay_bridge::Publish(snap);return;}
        const bool previously_ready=snap.ready;
        // Root handles may be replaced during discovery. Hooks must never read
        // an old handle while a managed call reenters this update.
        protect=false;ghost_enabled=ammo_enabled=false;protected_player=0;protected_manager=0;protected_attack=protected_state=protected_script=0;
        tool_movement=tool_fall=tool_crossbow=tool_shotgun=tool_inventory=0;
        if(!api.Value<bool>(queue,nullptr,false)){
            gameplay_bridge::Clear();snap.ready=snap.spawn_ready=snap.god_ready=snap.ghost_ready=snap.infinity_ready=snap.fall_ready=false;
            snap.god=snap.infinity=snap.explosion_ready=false;
            snap.status=u8"Oyun ağ kuyruğu duraklatıldı; kontroller bekliyor.";
            Diagnostics();gameplay_bridge::Publish(snap);return;
        }
        Root room(api,api.Call(current_room));int current_scene=0;
        const bool scene_ready=ReadScene(api,get_scene,scene_valid,scene_loaded,current_scene);
        if(!scene_ready){
            Clear(false);scene=-1;room_id=player_id=0;
            snap.status=u8"Oyun sahnesinin yüklenmesi bekleniyor.";
            Diagnostics();gameplay_bridge::Publish(snap);return;
        }
        const auto current_room_id=reinterpret_cast<uintptr_t>(room.Get());
        if(current_scene!=scene||current_room_id!=room_id){Clear(false);scene=current_scene;room_id=current_room_id;++generation;}
        const double now=gameplay_bridge::Now();
        if(now>=next_scan){
            Root candidate(api),candidate_manager(api);bool ambiguous=false,manager_ambiguous=false;
            int seen=0,hierarchical=0;
            const auto consider=[&](Object* o){
                if(!o||api.object_get_class(o)!=player_class)return;
                ++seen;
                if(!InHierarchy(o))return;
                ++hierarchical;
                if(!Owned(o,"qsa"))return;
                if(candidate.Get()&&candidate.Get()!=o)ambiguous=true;
                candidate.Set(o);
            };
            const bool player_scan=Each(player_class,consider);
            // A disabled controller may also be referenced by its own manager.
            const bool manager_scan=Each(manager_class,[&](Object* o){
                if(!InHierarchy(o)||!Owned(o,"raf"))return;
                Root controller(api,Ref(o,"playerControllerInstance"));consider(controller.Get());
                if(controller.Get()&&controller.Get()==candidate.Get()){
                    if(candidate_manager.Get()&&candidate_manager.Get()!=o)manager_ambiguous=true;
                    candidate_manager.Set(o);
                }
            });
            if(ambiguous)candidate.Set(nullptr);
            const auto id=reinterpret_cast<uintptr_t>(candidate.Get());
            if(id!=player_id){Clear(true);player_id=id;++generation;}
            player.Set(candidate.Get());manager.Set(Ref(player.Get(),"qry"));
            if(!manager.Get()&&player.Get()&&!manager_ambiguous)manager.Set(candidate_manager.Get());
            player_issue=ambiguous?u8"Birden fazla yerel maç karakteri var; yeniden doğma işlemi bekleniyor.":
                !player_scan&&!manager_scan?u8"Maç oyuncusu araması tamamlanamadı.":
                !seen?u8"Bu sahnede maç karakteri yok; bekleme odasında maç oyuncusu oluşmaz.":
                !hierarchical?u8"Maç karakterinin sahnede etkinleşmesi bekleniyor.":
                !player.Get()?u8"Maç karakterinin sana ait olduğu henüz doğrulanamadı.":"";
            Root inv(api);ambiguous=false;
            const bool inventory_scan=Each(inventory_class,[&](Object* o){if(InHierarchy(o)&&Owned(o,"pv")){if(inv.Get()&&inv.Get()!=o)ambiguous=true;inv.Set(o);}});
            inventory.Set(ambiguous?nullptr:inv.Get());Catalog();
            inventory_issue=ambiguous?u8"Birden fazla yerel envanter bulundu; yeniden doğma işlemi bekleniyor.":
                !inventory_scan?u8"Maç envanteri araması tamamlanamadı.":
                !inventory.Get()?u8"Kendi maç envanterin bulunamadı; eşya oluşturma maç içinde açılır.":"";
            FindOwned(attack_class,attacked);FindOwned(state_class,player_state);
            const auto attack_id=reinterpret_cast<uintptr_t>(attacked.Get()),inv_id=reinterpret_cast<uintptr_t>(inventory.Get());
            if(modern_id!=attack_id||inventory_id!=inv_id){DisableTools(true);for(auto& a:actors){Restore(*a);CombatGuard(*a,false);}settings={};snap.god=snap.ghost=snap.infinity=false;gameplay_bridge::Clear();++generation;modern_id=attack_id;inventory_id=inv_id;}
            FindOwned(script_class,script);FindOwned(movement_class,movement);FindOwned(fall_class,fall_damage);FindOwned(crossbow_class,crossbow);FindOwned(weapon_class,shotgun);
            Root next_sight(api);FindOwned(sight_class,next_sight);
            if(next_sight.Get()!=sight.Get()){ReleaseSight(true);sight.Set(next_sight.Get());}
            if(room.Get()){Discover();Capabilities();}
            next_scan=now+1;
        }
        const bool authority=room.Get()&&api.Value<bool>(master,nullptr,false);
        // Enemy controls need the room/scene and owned enemy, not a player
        // controller. Spawn and God Mode keep their own stricter requirements.
        snap.context=generation;snap.ready=room.Get()!=nullptr;
        const bool own_player=InHierarchy(player.Get())&&Owned(player.Get(),"qsa");
        const bool own_inventory=InHierarchy(inventory.Get())&&Owned(inventory.Get(),"pv");
        const bool own_manager=InHierarchy(manager.Get())&&Owned(manager.Get(),"raf");
        const bool own_modern=InHierarchy(attacked.Get())&&Owned(attacked.Get(),"pv")&&InHierarchy(player_state.Get())&&Owned(player_state.Get(),"pv");
        const bool own_movement=InHierarchy(movement.Get())&&Owned(movement.Get(),"pv");
        const bool has_modern=attacked.Get()||player_state.Get();
        snap.authority=authority;
        snap.player_speed_ready=PlayerSpeedReady();
        snap.hamarat_ready=assist_hooks_ready&&own_inventory;
        const bool in_match=own_inventory||own_modern||own_player;
        snap.difficulty_ready=in_match&&authority&&difficulty_field&&difficulty_refresh[0]&&difficulty_refresh[1]&&difficulty_helpers_ready[0]&&difficulty_helpers_ready[1];
        if(difficulty_field){uint8_t value=255;api.field_static_get_value(difficulty_field,&value);snap.difficulty=value<=5?value:-1;}
        snap.god_ready=snap.ready&&(has_modern?(own_modern&&modern_hooks_ready):(own_player&&god_hooks_ready&&own_manager));
        snap.fall_ready=snap.god_ready&&fall_god_ready&&InHierarchy(fall_damage.Get())&&Owned(fall_damage.Get(),"pv");
        snap.explosion_ready=snap.god_ready&&(has_modern?explosion_hooks_ready:legacy_explosion_ready);
        const bool spawn_api=spawn&&prefix_get&&path_combine&&pos_get&&rot_get&&name_get;
        snap.spawn_ready=snap.ready&&own_inventory&&spawn_api&&!prefabs.empty();
        snap.ghost_ready=snap.ready&&own_movement&&ghost_hook_ready&&InHierarchy(fall_damage.Get())&&Owned(fall_damage.Get(),"pv");
        snap.infinity_ready=snap.ready&&own_inventory&&ammo_hooks_ready;
        snap.tools_status=!own_movement?u8"Kendi hareket bileşenin bekleniyor.":!ghost_hook_ready?u8"Hayalet modu hareket bağlantısı eksik.":!ammo_hooks_ready?u8"Cephane bağlantılarının bir kısmı eksik.":u8"Kendi oyuncun için hareket ve cephane kontrolleri hazır.";
        snap.spawn_status=!snap.ready?u8"Bir oyun odasına gir.":!own_inventory?(inventory_issue.empty()?u8"Envanter sahipliği yeniden doğrulanıyor.":inventory_issue):
            !spawn_api?u8"Bu sürümün eşya oluşturma bağlantısı eksik.":prefabs.empty()?u8"Oyunun eşya prefab listesini yüklemesi bekleniyor.":u8"Eşya oluşturma hazır.";
        snap.god_status=!snap.ready?u8"Bir oyun odasına gir.":own_modern?(modern_hooks_ready?u8"Güncel oyuncu ve ölüm koruması hazır.":u8"Güncel oyuncu bulundu; ölüm koruması bağlantıları eksik."):(attacked.Get()||player_state.Get())?u8"Güncel karakterin saldırı ve durum bileşenlerinin sahipliği bekleniyor.":!own_player?(player_issue.empty()?u8"Oyuncu sahipliği yeniden doğrulanıyor.":player_issue):
            !own_manager?u8"Kendi karakterinin oyun yöneticisi bekleniyor.":!god_hooks_ready?u8"Ölüm koruması bağlantılarının tamamı kurulamadı.":u8"Kendi karakterin için God Mode hazır.";
        if(!snap.god_ready)snap.god=false;
        if(!snap.ghost_ready)snap.ghost=false;
        if(!snap.infinity_ready)snap.infinity=false;
        if(!snap.ready){protect=false;gameplay_bridge::Clear();snap.status=u8"Oyun odasına girmen bekleniyor.";}
        else if(!previously_ready||snap.status==u8"Oyun sahnesi bekleniyor.")snap.status=u8"Oda ve sahne hazır. Her kontrol kendi hedefi bulunduğunda açılır.";
        bool spawned_this_frame=false;
        for(const auto& command:gameplay_bridge::Take()){
            if(!snap.ready||command.context!=generation||now-command.queued>3)continue;
            if(command.kind==gameplay_bridge::Kind::StartVote){StartVote(command.item,command.seconds,now);continue;}
            if(command.kind==gameplay_bridge::Kind::CancelVote){
                if(VoteRoomReady()&&ballot.Active()){VoteNotice(ballot.name+u8" için oylama başlatan kişi tarafından iptal edildi.");EndVote(u8"Oylama iptal edildi; oyuncu atılmadı.");}continue;
            }
            if(command.kind==gameplay_bridge::Kind::Spawn){if(spawned_this_frame){gameplay_bridge::Queue(command);continue;}spawned_this_frame=true;Spawn(command.item);continue;}
            if(command.kind==gameplay_bridge::Kind::Difficulty){ChangeDifficulty(command.item);continue;}
            if(command.kind==gameplay_bridge::Kind::PlayerSpeed){
                if(snap.player_speed_ready&&std::isfinite(command.factor)){snap.player_speed=std::clamp(command.factor,1.f,5.f);snap.status=u8"Karakter hızı ayarlandı.";__android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG player-speed factor=%.2f",double(snap.player_speed));}continue;
            }
            if(command.kind==gameplay_bridge::Kind::God){
                SetGod(command.flag);continue;
            }
            if(command.kind==gameplay_bridge::Kind::Ghost){snap.ghost=command.flag&&snap.ghost_ready;snap.status=snap.ghost?u8"Hayalet modu açık; joystick ile duvarlardan geçebilirsin.":u8"Hayalet modu kapalı; normal çarpışma hareketi geri geldi.";continue;}
            if(command.kind==gameplay_bridge::Kind::GhostReturn){if(ReturnGhost())snap.status=u8"Hayalet moduna başladığın konuma döndün.";continue;}
            if(command.kind==gameplay_bridge::Kind::InfinityAmmo){snap.status=u8"Infinity Ammo, God Mode'a bağlı.";continue;}
            if(command.kind==gameplay_bridge::Kind::RemoveEnemy){Remove(command.enemy);continue;}
            if(command.kind==gameplay_bridge::Kind::KillEnemy){Kill(command.enemy);continue;}
            if(command.kind==gameplay_bridge::Kind::CloneEnemy){Clone(command.enemy,command.settings,command.clone_distance);continue;}
            if(command.kind==gameplay_bridge::Kind::PauseEnemy){
                if(command.enemy<3&&enemy_update_ready[command.enemy]){settings[command.enemy].paused=command.flag;snap.status=std::string(gameplay_bridge::kNames[command.enemy])+(command.flag?u8": türün tüm düşmanları durduruldu.":u8": türün tüm düşmanları devam ediyor.");}
                continue;
            }
            settings[command.enemy]=command.kind==gameplay_bridge::Kind::ResetEnemy?Enemy{}:command.settings;
            if(!enemy_update_ready[command.enemy])settings[command.enemy].paused=false;
            snap.status=authority?u8"Düşman ayarları uygulandı; sıfırlama özgün değerleri geri yükler.":u8"Düşman ayarları bu cihazda uygulandı; diğer oyunculardaki sonuç ağ yetkisine bağlıdır.";
        }
        protect=snap.god&&snap.god_ready;protected_player=player.h;protected_manager=manager.h;protected_scene=scene;protected_room=room_id;
        protected_attack=attacked.h;protected_state=player_state.h;protected_script=script.h;
        tool_movement=movement.h;tool_fall=fall_damage.h;tool_crossbow=crossbow.h;tool_shotgun=shotgun.h;tool_inventory=inventory.h;
        UpdateTools();if(snap.ready){ScanNoise(now);UpdateClones();UpdateVote(now);}
        if(snap.ready){for(auto& a:actors){Apply(*a);CombatGuard(*a,snap.god&&snap.god_ready);}AnimateClones();}
        for(size_t k=0;k<kEnemyCount;++k){const auto observed=snap.enemies[k];snap.enemies[k]=settings[k];snap.enemies[k].count=observed.count;snap.enemies[k].death_ready=observed.death_ready;snap.enemies[k].clone_ready=observed.clone_ready;snap.enemies[k].pause_ready=enemy_update_ready[k];}
        Diagnostics();gameplay_bridge::Publish(snap);
    }
};
Runtime* runtime=nullptr;
bool OverrideSight(Object* object,bool requested,bool remember){return runtime&&std::this_thread::get_id()==unity_thread&&runtime->OverrideSightRequest(object,requested,remember);}
bool IgnoreOwnCollider(Object* collider,size_t kind){
    if(kind>2||!runtime||std::this_thread::get_id()!=unity_thread||!protect||!collider)return false;
    Root target(runtime->api,runtime->api.Call(runtime->go,collider));return LocalVictim(target.Get());
}
bool CanAssistInventory(Object* object){return std::this_thread::get_id()==unity_thread&&assist_hooks_ready&&protect&&runtime&&runtime->snap.god&&runtime->snap.god_ready&&runtime->InHierarchy(object)&&ToolOwner(object,tool_inventory,inventory_view);}
bool AssistRequirement(Object* target,Object* requirements){return runtime&&runtime->AssistWorld(target,requirements);}
bool AssistedTarget(Object* target){return assist_scope&&assist_scope->assisted&&assist_scope->target.Get()==target&&CanAssistInventory(assist_scope->inventory.Get());}
void ReportFootstep(Object* object){if(runtime&&std::this_thread::get_id()==unity_thread)runtime->CaptureFootstep(object,gameplay_bridge::Now());}
void ReportNoise(Object* object){if(runtime&&std::this_thread::get_id()==unity_thread)runtime->CaptureNoise(object,gameplay_bridge::Now());}
void CompletedEnemyFrame(Object* object,size_t kind){
    if(std::this_thread::get_id()!=unity_thread||!runtime||kind>=kEnemyCount)return;
    for(const auto& c:runtime->clones)if(c->controller.Get()==object&&c->completed_frames<1000000){++c->completed_frames;runtime->DriveCloneInvestigation(*c,gameplay_bridge::Now());return;}
}
void GuardGodEnemy(Object* object,size_t kind){
    if(std::this_thread::get_id()!=unity_thread||!runtime||!runtime->snap.ready||kind>2)return;
    for(const auto& a:runtime->actors)if(a->kind==kind&&a->object.Get()==object){runtime->CombatGuard(*a,runtime->snap.god&&runtime->snap.god_ready);return;}
}
void VoteMessage(Object* sender,const std::string& line,int32_t sent){if(runtime&&std::this_thread::get_id()==unity_thread)runtime->IncomingVote(sender,line,sent);}
bool EnemyPaused(Object* object,size_t kind){
    if(std::this_thread::get_id()!=unity_thread||!runtime||!runtime->snap.ready||kind>=kEnemyCount||!runtime->settings[kind].paused||!WorldMatches())return false;
    for(const auto& actor:runtime->actors)if(actor->kind==kind&&actor->object.Get()==object)return true;
    return false;
}
bool FrozenEnemyPose(Object* t,bool local,Vec3* position,Quat* rotation){
    if(std::this_thread::get_id()!=unity_thread||!runtime||!runtime->snap.ready)return false;
    for(const auto& a:runtime->actors){
        if(!runtime->settings[a->kind].paused||!(local?a->local_pose_frozen:a->pose_frozen)||a->transform.Get()!=t)continue;
        if(!WorldMatches())return false;
        if(position)*position=local?a->frozen_local_position:a->frozen_position;
        if(rotation)*rotation=local?a->frozen_local_rotation:a->frozen_rotation;
        return true;
    }return false;
}
}
void Install(const Api& api){
    if(hooks_installed)return;
    hooks_installed=true;hook_api=&api;unity_thread=std::this_thread::get_id();
    if(!api.domain_get||!api.class_get_field_from_name||!api.field_get_value||!api.gchandle_new)return;
    scene_method=api.FindMethod(api.Find("UnityEngine.SceneManagement","SceneManager"),"GetActiveScene",0);if(!scene_method)return;
    auto* scene_class=api.Find("UnityEngine.SceneManagement","Scene");
    scene_valid_method=api.FindMethod(scene_class,"IsValidInternal",1);scene_loaded_method=api.FindMethod(scene_class,"GetIsLoadedInternal",1);
    room_method=api.FindMethod(api.Find("Photon.Pun","PhotonNetwork"),"get_CurrentRoom",0);
    mine_method=api.FindMethod(api.Find("Photon.Pun","PhotonView"),"get_IsMine",0);
    view_method=api.FindMethod(api.Find("Photon.Pun","MonoBehaviourPun"),"get_photonView",0);
    if(!room_method||!mine_method||!scene_valid_method||!scene_loaded_method)return;
    InstallModern(api);
    InstallExplosion(api);
    InstallTools(api);
    InstallSight(api);
    InstallProgression(api);
    InstallEnemyUpdates(api);
    footstep_hook_ready=Hook(api.FindMethod(api.Find("","FootStepsController"),"OnSteppedFloor",0),reinterpret_cast<void*>(FootstepEmitted),original_footstep);
    noise_hooks_ready[0]=Hook(api.FindMethod(api.Find("","NoiseObjectFallsMulti"),"ffp",0),reinterpret_cast<void*>(NoiseEmitted<0>),original_noise[0]);
    noise_hooks_ready[1]=Hook(api.FindMethod(api.Find("","NoiseObjectFallsMultiGrandpa"),"fga",0),reinterpret_cast<void*>(NoiseEmitted<1>),original_noise[1]);
    __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG build=voice-ai-panel-latebind-arm64 modern=%d gas=%d ammo=%d hamarat=%d fall=%d sound=%d%d steps=%d enemy_updates=%d%d%d%d%d%d",modern_hooks_ready,explosion_hooks_ready,ammo_hooks_ready,assist_hooks_ready,fall_god_ready,noise_hooks_ready[0],noise_hooks_ready[1],footstep_hook_ready,enemy_update_ready[0],enemy_update_ready[1],enemy_update_ready[2],enemy_update_ready[3],enemy_update_ready[4],enemy_update_ready[5]);
    InstallVoteReceiver(api);
    auto* player=api.Find("","playerController");auto* manager=api.Find("","playerManager");if(!player||!manager)return;
    player_view=api.class_get_field_from_name(player,"qsa");if(!room_method||!mine_method||(!player_view&&!view_method)||!scene_valid_method||!scene_loaded_method)return;
    const bool a=Hook(api.FindMethod(manager,"die",0),reinterpret_cast<void*>(Die),original_die);
    const bool b=Hook(api.FindMethod(player,"RPC_playerKilledByAI",0),reinterpret_cast<void*>(AiDeath),original_ai);
    const bool c=Hook(api.FindMethod(player,"RPC_playerDiesBySpider",0),reinterpret_cast<void*>(SpiderDeath),original_spider);
    const bool d=Hook(api.FindMethod(player,"RPC_playerDiesBySpiderAIGranny",0),reinterpret_cast<void*>(SpiderAiDeath),original_spider_ai);
    const bool e=Hook(api.FindMethod(player,"RPC_playerDies",1),reinterpret_cast<void*>(NamedDeath),original_named);
    bool f=false;
    if(api.class_get_nested_types){void* it=nullptr;for(int n=0;n<256;++n){
        auto* nested=api.class_get_nested_types(player,&it);if(!nested)break;
        if(std::strcmp(api.class_get_name(nested),"qr"))continue;
        mom_owner=api.class_get_field_from_name(nested,"qnk");
        if(mom_owner)f=Hook(api.FindMethod(nested,"MoveNext",0),reinterpret_cast<void*>(MomDeath),original_mom);
        break;
    }}
    god_hooks_ready=a&&b&&c&&d&&e&&f;
    __android_log_print(ANDROID_LOG_INFO,"multiplayermod","gameplay: owned-player death hooks %s",god_hooks_ready?"ready":"unavailable");
}
void Pump(const Api& api,bool paused){
    static bool stopped=false;if(stopped)return;
    if(!api.domain_get)return;
    try{if(!runtime)runtime=new Runtime(api);runtime->Tick(paused);}
    catch(...){stopped=true;protect=false;ghost_enabled=ammo_enabled=vote_listening=false;__android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG pump stopped: native C++ exception");gameplay_bridge::Snapshot s;s.status=u8"Oyun özellikleri hata nedeniyle durduruldu.";gameplay_bridge::Clear();gameplay_bridge::Publish(s);}
}
}
