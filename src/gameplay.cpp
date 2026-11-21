#include "gameplay.h"
#include "gameplay_bridge.h"
#include "gameplay_catalog.h"
#include "dobby.h"
#include <android/log.h>
#include <algorithm>
#include <array>
#include <cmath>
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
bool BlockHit(Object* o,Object* target){return LocalGodTarget(target)||(ModernProtected(o)&&target&&hook_api->Call(local_player_method)==target);}
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
Handle tool_movement=0,tool_fall=0,tool_crossbow=0,tool_shotgun=0,tool_inventory=0;
Field *movement_view=nullptr,*movement_controller=nullptr,*movement_velocity=nullptr,*fall_view=nullptr,
      *crossbow_view=nullptr,*crossbow_loaded=nullptr,*shotgun_view=nullptr,*shotgun_manager=nullptr,*shotgun_loaded=nullptr,*inventory_view=nullptr;
const Method *tool_transform=nullptr,*tool_position=nullptr,*tool_set_position=nullptr,*tool_delta=nullptr,*tool_hand=nullptr,*tool_go_transform=nullptr,*tool_is_child=nullptr;
Unary original_motion=nullptr,original_fall_update=nullptr,original_cross_fire=nullptr,original_shot_fire=nullptr;
using VictimAction=void(*)(Object*,Object*,const Method*);
VictimAction original_mom_grab=nullptr;
bool Finite(Vec3 v);
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
Vec3 GhostMotion(Vec3 velocity,float delta){return {velocity.x*delta,0,velocity.z*delta};}
void Motion(Object* o,const Method* m){
    if(std::this_thread::get_id()==unity_thread&&ghost_enabled&&movement_velocity&&ToolOwner(o,tool_movement,movement_view)){
        Vec3 velocity{};hook_api->field_get_value(o,movement_velocity,&velocity);
        const float delta=hook_api->Value<float>(tool_delta,nullptr,-1);
        Object* body=nullptr;hook_api->field_get_value(o,movement_controller,&body);Root controller(*hook_api,body),t(*hook_api,hook_api->Call(tool_transform,controller.Get()));
        bool ok=false;Root boxed(*hook_api,hook_api->Call(tool_position,t.Get(),nullptr,&ok));
        if(ok&&boxed.Get()&&Finite(velocity)&&std::isfinite(delta)&&delta>=0&&delta<=1){
            Vec3 p=*static_cast<Vec3*>(hook_api->object_unbox(boxed.Get())),step=GhostMotion(velocity,delta);p.x+=step.x;p.z+=step.z;
            if(Finite(p)){void* args[]={&p};hook_api->Call(tool_set_position,t.Get(),args,&ok);if(ok){velocity.y=0;hook_api->field_set_value(o,movement_velocity,&velocity);return;}}
        }
    }
    original_motion(o,m);
}
void FallUpdate(Object* o,const Method* m){if(std::this_thread::get_id()!=unity_thread||!ghost_enabled||!ToolOwner(o,tool_fall,fall_view))original_fall_update(o,m);}
bool FieldType(const Api& api,Field* field,const char* ns,const char* name){
    if(!field||!api.field_get_type||!api.class_from_type)return false;
    auto* c=api.class_from_type(api.field_get_type(field));return c&&!std::strcmp(api.class_get_namespace(c),ns)&&!std::strcmp(api.class_get_name(c),name);
}
void InfiniteFire(Object* o,const Method* m,bool cross){
    const auto original=cross?original_cross_fire:original_shot_fire;
    if(std::this_thread::get_id()!=unity_thread||!ammo_enabled||!ToolOwner(o,cross?tool_crossbow:tool_shotgun,cross?crossbow_view:shotgun_view)||!tool_inventory){original(o,m);return;}
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
    movement_view=field(movement,"pv");movement_controller=field(movement,"controller");movement_velocity=field(movement,"rdp");fall_view=field(fall,"pv");
    crossbow_view=field(cross,"pv");crossbow_loaded=field(cross,"haveSyringe");shotgun_view=field(shot,"pv");shotgun_manager=field(shot,"shotgunManager");shotgun_loaded=field(manager,"shotgunLoaded");inventory_view=field(inv,"pv");
    const auto method=[&](const char* ns,const char* c,const char* n,int count=0){return api.FindMethod(api.Find(ns,c),n,count);};
    tool_transform=method("UnityEngine","Component","get_transform");tool_position=method("UnityEngine","Transform","get_position");tool_set_position=method("UnityEngine","Transform","set_position",1);tool_delta=method("UnityEngine","Time","get_deltaTime");tool_hand=api.FindMethod(inv,"hrl",0);
    tool_go_transform=method("UnityEngine","GameObject","get_transform");tool_is_child=method("UnityEngine","Transform","IsChildOf",1);
    const bool mom_guard=Hook(api.FindMethod(api.Find("","SpiderMomAIController"),"inh",1),reinterpret_cast<void*>(MomGrab),original_mom_grab);
    modern_hooks_ready=modern_hooks_ready&&mom_guard;
    if(movement_view&&movement_controller&&fall_view&&tool_transform&&tool_position&&tool_set_position&&tool_delta&&FieldType(api,movement_velocity,"UnityEngine","Vector3")){
        const bool a=Hook(api.FindMethod(movement,"hjs",0),reinterpret_cast<void*>(Motion),original_motion);
        const bool b=Hook(api.FindMethod(fall,"Update",0),reinterpret_cast<void*>(FallUpdate),original_fall_update);ghost_hook_ready=a&&b;
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
    bool nav_changed=false,anim_changed=false,enabled_changed=false,enabled_before=true;
    bool stop_changed=false,stop_before=false,scale_changed=false;
    Vec3 scale_before{},scale_last{};
    std::vector<std::unique_ptr<AnimationNode>> animations;
    std::vector<std::unique_ptr<ScaleNode>> visuals;
    Field* no_hit=nullptr;bool no_hit_changed=false,no_hit_before=false;
    Actor(const Api& a,Object* p,size_t k):object(a,p),nav(a),animator(a),transform(a),kind(k){}
};
struct Prefab {int id;Root object;std::string name;bool local=false,pickup=false;Prefab(const Api& a,int i,Object* o,std::string n,bool l=false,bool p=false):id(i),object(a,o),name(std::move(n)),local(l),pickup(p){} };
struct LocalAmmo {Root object;double born=0;bool requested=false;LocalAmmo(const Api& a,Object* o,double now):object(a,o),born(now){}};
struct Runtime {
    const Api& api;
    bool initialized=false,valid=false,busy=false;
    int scene=-1;uintptr_t room_id=0,player_id=0,modern_id=0,inventory_id=0;uint64_t generation=0;
    double next_scan=0;
    Root player,manager,inventory,attacked,player_state,script,movement,fall_damage,crossbow,shotgun,ammo_template;
    std::array<Class*,kEnemyCount> classes{};
    std::array<Enemy,kEnemyCount> settings{};
    std::vector<std::unique_ptr<Actor>> actors;
    std::vector<std::unique_ptr<Prefab>> prefabs;
    std::vector<std::unique_ptr<Root>> clones;
    std::vector<std::unique_ptr<LocalAmmo>> local_ammo;
    bool ghost_active=false,cross_visual=false,shot_visual=false,cross_button_saved=false,shot_button_saved=false;
    Vec3 ghost_origin{};
    gameplay_bridge::Snapshot snap;
    std::string player_issue=u8"Maç karakteri bekleniyor.",inventory_issue=u8"Maç envanteri bekleniyor.";
    const Method *find{},*length{},*item{},*alive{},*active{},*enabled_get{},*enabled_set{},*go{},*go_active{},
        *transform{},*scale_get{},*scale_set{},*nav_get{},*nav_set{},*nav_stop_get{},*nav_stop_set{},*nav_onmesh{},
        *anim_get{},*anim_set{},*current_room{},*master{},*queue{},*get_scene{},*mine{},*name_get{},
        *pos_get{},*rot_get{},*spawn{},*prefix_get{},*path_combine{},*network_destroy{},*local_destroy{},*view_get{},*scene_valid{},*scene_loaded{},
        *children{},*go_transform{},*is_child{},*hand_get{},*hand_pick{},*resources_load{},*local_clone{},*rpc{},*view_id{},*find_all{},*nav_sample{},*pos_set{},*rot_set{},*active_set{},*active_get{},*cross_images{},*cross_arrow{},
        *go_ctor{},*add_component{},*mesh_get{},*mesh_set{},*materials_get{},*materials_set{},*lossy_scale{};
    Class *player_class{},*manager_class{},*inventory_class{},*tie_class{},*attack_class{},*state_class{},*animator_class{},*animation_class{},*view_class{},*shotgun_class{},*pun_class{},
        *script_class{},*movement_class{},*fall_class{},*crossbow_class{},*weapon_class{},*pickup_class{},*renderer_class{},*mono_class{},*go_class{},*mesh_filter_class{},*mesh_renderer_class{};
    std::array<Class*,3> clone_managers{};
    explicit Runtime(const Api& a):api(a),player(a),manager(a),inventory(a),attacked(a),player_state(a),script(a),movement(a),fall_damage(a),crossbow(a),shotgun(a),ammo_template(a){}
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
    const Method* ExactClone(){
        if(!api.class_get_methods||!api.method_get_param_count||!api.method_get_param)return nullptr;
        auto* c=api.Find("UnityEngine","Object");if(!c)return nullptr;
        void* it=nullptr;const Method* found=nullptr;const char* types[]={"Object","Vector3","Quaternion"};
        for(int n=0;n<1024;++n){auto* m=api.class_get_methods(c,&it);if(!m)break;
            if(std::strcmp(api.method_get_name(m),"Instantiate")||api.method_get_param_count(m)!=3)continue;
            if(api.method_is_generic&&api.method_is_generic(m))continue;
            bool match=true;for(uint32_t i=0;i<3;++i){auto* type=api.class_from_type(api.method_get_param(m,i));if(!type||std::strcmp(api.class_get_namespace(type),"UnityEngine")||std::strcmp(api.class_get_name(type),types[i]))match=false;}
            if(match){if(found)return nullptr;found=m;}
        }return found;
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
    bool Resolve(){
        if(!api.domain_get||!api.field_get_value||!api.field_set_value||!api.field_get_type||!api.class_from_type||
           !api.object_get_class||!api.class_get_field_from_name||!api.class_get_type||!api.type_get_object)return false;
        find=ResolveMethod("UnityEngine","Object","FindObjectsByType",3);length=ResolveMethod("System","Array","get_Length");item=ResolveMethod("System","Array","GetValueImpl",1);
        alive=ResolveMethod("UnityEngine","Object","op_Implicit",1);active=ResolveMethod("UnityEngine","Behaviour","get_isActiveAndEnabled");
        enabled_get=ResolveMethod("UnityEngine","Behaviour","get_enabled");enabled_set=ResolveMethod("UnityEngine","Behaviour","set_enabled",1);
        go=ResolveMethod("UnityEngine","Component","get_gameObject");go_active=ResolveMethod("UnityEngine","GameObject","get_activeInHierarchy");transform=ResolveMethod("UnityEngine","Component","get_transform");
        scale_get=ResolveMethod("UnityEngine","Transform","get_localScale");scale_set=ResolveMethod("UnityEngine","Transform","set_localScale",1);
        nav_get=ResolveMethod("UnityEngine.AI","NavMeshAgent","get_speed");nav_set=ResolveMethod("UnityEngine.AI","NavMeshAgent","set_speed",1);
        nav_stop_get=ResolveMethod("UnityEngine.AI","NavMeshAgent","get_isStopped");nav_stop_set=ResolveMethod("UnityEngine.AI","NavMeshAgent","set_isStopped",1);nav_onmesh=ResolveMethod("UnityEngine.AI","NavMeshAgent","get_isOnNavMesh");
        anim_get=ResolveMethod("UnityEngine","Animator","get_speed");anim_set=ResolveMethod("UnityEngine","Animator","set_speed",1);
        current_room=ResolveMethod("Photon.Pun","PhotonNetwork","get_CurrentRoom");master=ResolveMethod("Photon.Pun","PhotonNetwork","get_IsMasterClient");queue=ResolveMethod("Photon.Pun","PhotonNetwork","get_IsMessageQueueRunning");
        get_scene=ResolveMethod("UnityEngine.SceneManagement","SceneManager","GetActiveScene");mine=ResolveMethod("Photon.Pun","PhotonView","get_IsMine");
        scene_valid=ResolveMethod("UnityEngine.SceneManagement","Scene","IsValidInternal",1);scene_loaded=ResolveMethod("UnityEngine.SceneManagement","Scene","GetIsLoadedInternal",1);
        view_get=ResolveMethod("Photon.Pun","MonoBehaviourPun","get_photonView");
        children=ResolveMethod("UnityEngine","GameObject","GetComponentsInternal",6);go_transform=ResolveMethod("UnityEngine","GameObject","get_transform");is_child=ResolveMethod("UnityEngine","Transform","IsChildOf",1);
        hand_get=ResolveMethod("","HandlePickUpItemInteractions","hrl");hand_pick=ResolveMethod("","HandlePickUpItemInteractions","hra",2);
        resources_load=ResolveMethod("UnityEngine","Resources","Load",2);local_clone=ExactClone();
        rpc=ExactRpc();
        view_id=ResolveMethod("Photon.Pun","PhotonView","get_ViewID");
        find_all=ResolveMethod("UnityEngine","Resources","FindObjectsOfTypeAll",1);nav_sample=ExactNavSample();
        pos_set=ResolveMethod("UnityEngine","Transform","set_position",1);rot_set=ResolveMethod("UnityEngine","Transform","set_rotation",1);active_set=ResolveMethod("UnityEngine","GameObject","SetActive",1);active_get=ResolveMethod("UnityEngine","GameObject","get_activeSelf");
        cross_images=ResolveMethod("","HandleCrossBow","ice",1);cross_arrow=ResolveMethod("","HandleCrossBow","icg",1);
        go_class=api.Find("UnityEngine","GameObject");mesh_filter_class=api.Find("UnityEngine","MeshFilter");mesh_renderer_class=api.Find("UnityEngine","MeshRenderer");
        go_ctor=api.FindMethod(go_class,".ctor",1);add_component=api.FindMethod(go_class,"Internal_AddComponentWithType",1);
        mesh_get=api.FindMethod(mesh_filter_class,"get_sharedMesh",0);mesh_set=api.FindMethod(mesh_filter_class,"set_sharedMesh",1);
        materials_get=ResolveMethod("UnityEngine","Renderer","get_sharedMaterials");materials_set=ResolveMethod("UnityEngine","Renderer","set_sharedMaterials",1);lossy_scale=ResolveMethod("UnityEngine","Transform","get_lossyScale");
        name_get=ResolveMethod("UnityEngine","Object","get_name");pos_get=ResolveMethod("UnityEngine","Transform","get_position");rot_get=ResolveMethod("UnityEngine","Transform","get_rotation");
        spawn=ResolveMethod("Photon.Pun","PhotonNetwork","Instantiate",5);path_combine=ResolveMethod("System.IO","Path","Combine",2);
        prefix_get=ResolveMethod("<PrivateImplementationDetails>{B6FBF95C-D343-4B5E-9090-977A04841BC0}","a","bvx");
        network_destroy=ExactDestroy();local_destroy=ResolveMethod("UnityEngine","Object","Destroy",1);
        player_class=api.Find("","playerController");manager_class=api.Find("","playerManager");inventory_class=api.Find("","HandlePickUpItemInteractions");tie_class=api.Find("","HandItemTie");
        attack_class=api.Find("","HandleEnemyAttacked");state_class=api.Find("","PlayerStateManager");shotgun_class=api.Find("","ShotgunManager");
        animator_class=api.Find("UnityEngine","Animator");animation_class=api.Find("UnityEngine","Animation");view_class=api.Find("Photon.Pun","PhotonView");
        pun_class=api.Find("Photon.Pun","MonoBehaviourPun");
        script_class=api.Find("","PlayerScriptManager");movement_class=api.Find("","FirstPersonController");fall_class=api.Find("","HandleFallDemage");crossbow_class=api.Find("","HandleCrossBow");weapon_class=api.Find("","HandleShotgun");pickup_class=api.Find("","PickupItem");renderer_class=api.Find("UnityEngine","Renderer");mono_class=api.Find("UnityEngine","MonoBehaviour");
        clone_managers={api.Find("","GrannyManager"),api.Find("","GrandpaManager"),api.Find("","SpiderMomManager")};
        for(size_t k=0;k<kEnemyCount;++k)classes[k]=api.Find("",specs[k].klass);
        const Method* required[]={find,length,item,alive,active,enabled_get,enabled_set,go,go_active,transform,scale_get,scale_set,current_room,master,queue,get_scene,scene_valid,scene_loaded,mine};
        return (player_class||attack_class)&&std::all_of(std::begin(required),std::end(required),[](auto* m){return m!=nullptr;});
    }
    void Clear(bool restore){
        protect=false;ghost_enabled=ammo_enabled=false;protected_player=0;protected_manager=0;protected_attack=protected_state=protected_script=0;
        tool_movement=tool_fall=tool_crossbow=tool_shotgun=tool_inventory=0;
        DisableTools(restore);
        if(restore){for(auto& actor:actors){Restore(*actor);CombatGuard(*actor,false);}for(auto& ammo:local_ammo)if(Alive(ammo->object.Get())){void* args[]={ammo->object.Get()};api.Call(local_destroy,nullptr,args);}}
        if(restore&&Alive(ammo_template.Get())){void* args[]={ammo_template.Get()};api.Call(local_destroy,nullptr,args);}
        ammo_template.Set(nullptr);
        actors.clear();prefabs.clear();clones.clear();player.Set(nullptr);manager.Set(nullptr);inventory.Set(nullptr);attacked.Set(nullptr);player_state.Set(nullptr);
        script.Set(nullptr);movement.Set(nullptr);fall_damage.Set(nullptr);crossbow.Set(nullptr);shotgun.Set(nullptr);local_ammo.clear();
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
            else if(n->frozen){if(!api.Value<bool>(enabled_get,n->object.Get(),true))Set(enabled_set,n->object.Get(),n->enabled);n->frozen=false;}
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
        FreezeAnimations(a,false);ScaleVisuals(a,1);
        const bool fields_changed=std::any_of(a.speed_fields.begin(),a.speed_fields.end(),[](const auto& f){return f.changed;})||
            std::any_of(a.animation_fields.begin(),a.animation_fields.end(),[](const auto& f){return f.changed;});
        if(!fields_changed&&!a.enabled_changed&&!a.nav_changed&&!a.anim_changed&&!a.stop_changed&&!a.scale_changed)return;
        if(!Alive(a.object.Get()))return;
        ScalarApply(a,a.speed_fields,1);ScalarApply(a,a.animation_fields,1);
        if(a.enabled_changed){Set(enabled_set,a.object.Get(),a.enabled_before);a.enabled_changed=false;}
        if(Alive(a.nav.Get())&&api.Value<bool>(nav_onmesh,a.nav.Get(),false)){
            if(a.nav_changed&&Near(api.Value<float>(nav_get,a.nav.Get(),-1),a.nav_last))Set(nav_set,a.nav.Get(),a.nav_base);
            if(a.stop_changed)Set(nav_stop_set,a.nav.Get(),a.stop_before);
        }
        if(Alive(a.animator.Get())&&a.anim_changed&&Near(api.Value<float>(anim_get,a.animator.Get(),-1),a.anim_last))Set(anim_set,a.animator.Get(),a.anim_base);
        if(Alive(a.transform.Get())&&a.scale_changed&&Near(api.Value<Vec3>(scale_get,a.transform.Get()),a.scale_last))Set(scale_set,a.transform.Get(),a.scale_before);
        a.nav_changed=a.anim_changed=a.stop_changed=a.scale_changed=false;
    }
    void Apply(Actor& a){
        const auto& cfg=settings[a.kind];
        if(!cfg.paused&&Near(cfg.speed,1)&&Near(cfg.animation,1)&&Near(cfg.size,1)){Restore(a);return;}
        if(!Alive(a.object.Get()))return;
        ScalarApply(a,a.speed_fields,cfg.speed);ScalarApply(a,a.animation_fields,cfg.animation);
        FreezeAnimations(a,cfg.paused);ScaleVisuals(a,cfg.size);
        if(cfg.paused&&!a.enabled_changed){a.enabled_before=api.Value<bool>(enabled_get,a.object.Get(),false);a.enabled_changed=Set(enabled_set,a.object.Get(),false);}
        if(!cfg.paused&&a.enabled_changed){Set(enabled_set,a.object.Get(),a.enabled_before);a.enabled_changed=false;}
        if(Alive(a.nav.Get())&&nav_get&&nav_set&&api.Value<bool>(nav_onmesh,a.nav.Get(),false)){
            const float now=api.Value<float>(nav_get,a.nav.Get(),-1);
            if(std::isfinite(now)&&now>=0){
                if(!a.nav_changed||!Near(now,a.nav_last))a.nav_base=now;
                const float value=std::clamp(a.nav_base*cfg.speed,0.f,100.f);
                if(Set(nav_set,a.nav.Get(),value)){a.nav_last=value;a.nav_changed=true;}
            }
            if(cfg.paused&&!a.stop_changed){a.stop_before=api.Value<bool>(nav_stop_get,a.nav.Get(),false);a.stop_changed=Set(nav_stop_set,a.nav.Get(),true);}
            if(!cfg.paused&&a.stop_changed){Set(nav_stop_set,a.nav.Get(),a.stop_before);a.stop_changed=false;}
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
                if(k<2){auto* field=api.class_get_field_from_name(api.object_get_class(o),"dontHitPlayer");if(FieldType(api,field,"System","Boolean"))actor->no_hit=field;}
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
    Object* AddBuiltin(Object* object,Class* type){
        if(!type||!add_component)return nullptr;
        Root reflected(api,api.type_get_object(api.class_get_type(type)));if(!reflected.Get())return nullptr;void* args[]={reflected.Get()};return api.Call(add_component,object,args);
    }
    Object* BuildAmmoVisual(Object* source){
        if(Alive(ammo_template.Get()))return ammo_template.Get();
        if(!api.object_new||!go_class||!go_ctor||!active_set||!mesh_filter_class||!mesh_renderer_class||!mesh_get||!mesh_set||!materials_get||!materials_set||!add_component)return nullptr;
        Root mesh(api),materials(api),source_transform(api);
        Children(source,mesh_filter_class,[&](Object* filter){if(mesh.Get())return;Root found_mesh(api,api.Call(mesh_get,filter)),object(api,api.Call(go,filter));if(!Alive(found_mesh.Get())||!Alive(object.Get()))return;
            Root found_materials(api);Children(object.Get(),renderer_class,[&](Object* renderer){if(!found_materials.Get())found_materials.Set(api.Call(materials_get,renderer));});
            if(!found_materials.Get())return;
            mesh.Set(found_mesh.Get());materials.Set(found_materials.Get());source_transform.Set(api.Call(transform,filter));
        });
        if(!mesh.Get()||!materials.Get())return nullptr;
        Root name(api,reinterpret_cast<Object*>(api.string_new("Shotgun Ammo"))),object(api,api.object_new(go_class));if(!name.Get()||!object.Get())return nullptr;
        bool ok=false;void* ctor_args[]={name.Get()};api.Call(go_ctor,object.Get(),ctor_args,&ok);if(!ok||!Alive(object.Get()))return nullptr;
        const auto cleanup=[&](){void* args[]={object.Get()};api.Call(local_destroy,nullptr,args);};
        if(!Set(active_set,object.Get(),false)){cleanup();return nullptr;}
        Root filter(api,AddBuiltin(object.Get(),mesh_filter_class)),renderer(api,AddBuiltin(object.Get(),mesh_renderer_class));
        if(!Alive(filter.Get())||!Alive(renderer.Get())){cleanup();return nullptr;}
        void* mesh_args[]={mesh.Get()};api.Call(mesh_set,filter.Get(),mesh_args,&ok);if(!ok){cleanup();return nullptr;}
        void* material_args[]={materials.Get()};api.Call(materials_set,renderer.Get(),material_args,&ok);if(!ok){cleanup();return nullptr;}
        Root t(api,api.Call(go_transform,object.Get()));const Vec3 scale=api.Value<Vec3>(lossy_scale?lossy_scale:scale_get,source_transform.Get(),Vec3{1,1,1});if(Finite(scale))Set(scale_set,t.Get(),scale);
        ammo_template.Set(object.Get());return ammo_template.Get();
    }
    Object* VisualAmmoSource(Object* source){
        Root result(api);
        const auto accept=[&](Object* object){
            bool has_script=false,has_view=false;
            if(!Children(object,mono_class,[&](Object*){has_script=true;})||!Children(object,view_class,[&](Object*){has_view=true;})||has_script||has_view)return;
            result.Set(object);
        };
        accept(source);
        if(!result.Get())Children(source,renderer_class,[&](Object* renderer){if(result.Get())return;Root object(api,api.Call(go,renderer));if(Alive(object.Get()))accept(object.Get());});
        if(!result.Get())result.Set(BuildAmmoVisual(source));
        return result.Get();
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
        Root visual(api);std::string visual_name;
        for(const auto& source:sources){
            Root text(api,api.Call(name_get,source->Get()));if(!text.Get())continue;
            std::string name=api.Utf8(reinterpret_cast<String*>(text.Get()));if(name.empty()||name.size()>160)continue;
            const std::string suffix="(Clone)";if(name.size()>suffix.size()&&name.compare(name.size()-suffix.size(),suffix.size(),suffix)==0)name.resize(name.size()-suffix.size());
            Root asset(api);
            if(resources_load&&prefix_get&&path_combine){Root path(api,PathFor(name));auto* type=api.Find("UnityEngine","GameObject");Root reflection(api,type?api.type_get_object(api.class_get_type(type)):nullptr);
                if(path.Get()&&reflection.Get()){void* args[]={path.Get(),reflection.Get()};asset.Set(api.Call(resources_load,nullptr,args));}
            }
            if(Alive(asset.Get())&&HasAmmo(asset.Get())){bool has_view=false;if(Children(asset.Get(),view_class,[&](Object*){has_view=true;})&&has_view){prefabs.push_back(std::make_unique<Prefab>(api,48,asset.Get(),name));return;}}
            if(!visual.Get()&&local_clone&&HasAmmo(source->Get())){visual.Set(VisualAmmoSource(source->Get()));if(visual.Get())visual_name=name;}
        }
        if(visual.Get())prefabs.push_back(std::make_unique<Prefab>(api,48,visual.Get(),visual_name,true,true));
    }
    Object* Create(Prefab& prefab,Vec3 position,Quat rotation,bool& ok){
        ok=false;void* local_args[]={prefab.object.Get(),&position,&rotation};
        if(prefab.local){Root result(api,api.Call(local_clone,nullptr,local_args,&ok));if(ok&&Alive(result.Get())){bool on=true;void* args[]={&on};api.Call(ResolveMethod("UnityEngine","GameObject","SetActive",1),result.Get(),args,&ok);}return result.Get();}
        Root path(api,PathFor(prefab.name));if(!path.Get())return nullptr;
        uint8_t group=0;void* args[]={path.Get(),&position,&rotation,&group,nullptr};return api.Call(spawn,nullptr,args,&ok);
    }
    void Spawn(int id){
        if(!snap.spawn_ready){snap.status=snap.spawn_status;return;}
        const auto it=std::find_if(prefabs.begin(),prefabs.end(),[&](const auto& p){return p->id==id;});
        if(it==prefabs.end()){snap.status=id==48?u8"Shotgun Ammo listede; gerçek mermi modeli henüz yüklenmedi.":u8"Eşya kaynağı bulunamadı.";return;}
        if(!InHierarchy(inventory.Get())||!Owned(inventory.Get(),"pv")){snap.status=u8"Eşya seçimi bu oyuncu oturumuna ait değil.";return;}
        if(id==48&&(*it)->pickup&&local_ammo.size()>=64){snap.status=u8"Bu oturumdaki yerel mermi sınırı dolu (64). Önce mevcut mermilerden al.";return;}
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
        if(ok&&Alive(created.Get())&&(*it)->pickup){local_ammo.push_back(std::make_unique<LocalAmmo>(api,created.Get(),gameplay_bridge::Now()));snap.status=u8"Shotgun Ammo önüne kondu. Pompalı elindeyken yaklaşınca normal doldurma işlemi çalışır; bu mermi yereldir.";}
        else snap.status=ok&&Alive(created.Get())?u8"Eşya önünde oluşturuldu; normal ağ oluşturma yolu kullanıldı.":u8"Oyun bu prefabı oluşturamadı; konum veya kaynak kaydı hazır olmayabilir.";
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
    bool MoveGhost(float step,bool return_to_start=false){
        if(!snap.ghost_ready||!ghost_active)return false;
        Root body(api,BodyTransform());Vec3 p{};Quat q{};if(!Pose(body.Get(),p,q))return false;
        if(return_to_start)p=ghost_origin;else p.y+=step;
        if(!Finite(p)||!Set(pos_set,body.Get(),p))return false;
        if(return_to_start)snap.ghost=false;
        return true;
    }
    void ResetVerticalVelocity(){
        if(!movement_velocity||!Alive(movement.Get()))return;
        Vec3 v{};api.field_get_value(movement.Get(),movement_velocity,&v);if(Finite(v)){v.y=0;api.field_set_value(movement.Get(),movement_velocity,&v);}
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
    void DisableTools(bool restore){
        ghost_enabled=ammo_enabled=false;
        if(restore){if(ghost_active)ResetVerticalVelocity();AmmoVisuals(false);}
        ghost_active=cross_visual=shot_visual=false;
    }
    void UpdateTools(){
        if(snap.ghost&&!ghost_active){Root body(api,BodyTransform());Quat q{};if(Pose(body.Get(),ghost_origin,q)){ghost_active=true;ResetVerticalVelocity();}else{snap.ghost=false;snap.status=u8"Hayalet modu başlangıç konumunu okuyamadı.";}}
        if(!snap.ghost&&ghost_active){ResetVerticalVelocity();ghost_active=false;}
        AmmoVisuals(snap.infinity&&snap.infinity_ready);
        ghost_enabled=snap.ghost&&ghost_active&&snap.ghost_ready;ammo_enabled=snap.infinity&&snap.infinity_ready;
    }
    void CombatGuard(Actor& actor,bool enabled){
        if(!enabled&&!actor.no_hit_changed)return;
        if(!actor.no_hit||!Alive(actor.object.Get()))return;
        bool local_target=false;
        if(enabled){Root target(api,Ref(actor.object.Get(),"targetTransform"));if(!target.Get())target.Set(Ref(actor.object.Get(),"target"));Root body(api,BodyTransform());
            if(Alive(target.Get())&&Alive(body.Get())){void* args[]={body.Get()};bool ok=false;Root match(api,api.Call(is_child,target.Get(),args,&ok));local_target=target.Get()==body.Get()||(ok&&match.Get()&&*static_cast<bool*>(api.object_unbox(match.Get())));}
        }
        if(local_target){const bool now=ReadBool(actor.object.Get(),actor.no_hit);if(!actor.no_hit_changed||!now)actor.no_hit_before=now;bool yes=true;api.field_set_value(actor.object.Get(),actor.no_hit,&yes);actor.no_hit_changed=true;}
        else if(actor.no_hit_changed){if(ReadBool(actor.object.Get(),actor.no_hit))api.field_set_value(actor.object.Get(),actor.no_hit,&actor.no_hit_before);actor.no_hit_changed=false;}
    }
    void CollectAmmo(){
        if(local_ammo.empty())return;
        if(!inventory.Get()||!Owned(inventory.Get(),"pv")||!hand_pick||!shotgun_loaded)return;
        Root manager(api,Ref(inventory.Get(),"shotgunManager")),body(api,BodyTransform());Vec3 player_position{};Quat rotation{};if(!manager.Get()||!Pose(body.Get(),player_position,rotation))return;
        const bool loaded=ReadBool(manager.Get(),shotgun_loaded);const auto now=gameplay_bridge::Now();
        for(auto& token:local_ammo){
            if(!Alive(token->object.Get()))continue;
            Root t(api,api.Call(go_transform,token->object.Get()));Vec3 p{};Quat q{};if(!Pose(t.Get(),p,q))continue;
            const float dx=p.x-player_position.x,dz=p.z-player_position.z,dy=p.y-player_position.y;
            if(token->requested&&loaded){void* args[]={token->object.Get()};api.Call(local_destroy,nullptr,args);token->object.Set(nullptr);continue;}
            if(dx*dx+dz*dz>2.25f||std::fabs(dy)>2){token->requested=false;continue;}
            if(!loaded&&!token->requested&&now-token->born>.5&&api.Value<uint8_t>(hand_get,inventory.Get(),0)==25){uint8_t id=48;void* args[]={&id,nullptr};bool ok=false;api.Call(hand_pick,inventory.Get(),args,&ok);token->requested=ok;break;}
        }
        local_ammo.erase(std::remove_if(local_ammo.begin(),local_ammo.end(),[&](const auto& p){return !Alive(p->object.Get());}),local_ammo.end());
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
    void Clone(size_t kind){
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
        Vec3 position{};Quat q{};if(!InFront(position,q)){snap.status=u8"Kendi oyuncunun ön konumu okunamadı.";return;}
        NavHit hit{};float radius=1.8f;int areas=-1;void* sample[]={&position,&hit,&radius,&areas};bool sampled=false;Root found(api,api.Call(nav_sample,nullptr,sample,&sampled));
        const float dx=hit.position.x-position.x,dz=hit.position.z-position.z;
        if(!sampled||!found.Get()||!*static_cast<bool*>(api.object_unbox(found.Get()))||!hit.hit||!Finite(hit.position)||dx*dx+dz*dz>.35f*.35f){snap.status=u8"Tam önünde yürünebilir zemin bulunamadı; klon oluşturulmadı.";return;}
        position=hit.position;
        Prefab source(api,0,prefab.Get(),name);bool ok=false;Root created(api,Create(source,position,q,ok));
        if(!ok||!Alive(created.Get())){snap.status=u8"Oyun klon prefabını oluşturmayı reddetti.";return;}
        Root controller(api);bool ambiguous_controller=false;
        Children(created.Get(),classes[kind],[&](Object* o){if(controller.Get()&&controller.Get()!=o)ambiguous_controller=true;controller.Set(o);});
        // Serialized controller.pv may be initialized by Start next frame.
        // Validate the freshly instantiated hierarchy's actual registered view.
        Root pv(api);Children(created.Get(),view_class,[&](Object* view){if(!pv.Get()&&api.Value<int>(view_id,view,0)>0&&api.Value<bool>(mine,view,false))pv.Set(view);});
        const int id=pv.Get()?api.Value<int>(view_id,pv.Get(),0):0;
        if(!controller.Get()||ambiguous_controller||!Alive(pv.Get())||id<=0||!api.Value<bool>(mine,pv.Get(),false)){
            void* args[]={created.Get()};api.Call(id>0&&api.Value<bool>(mine,pv.Get(),false)?network_destroy:local_destroy,nullptr,args);
            snap.status=u8"Klonun AI/ağ kimliği doğrulanamadı; oluşturulan nesne temizlendi.";return;
        }
        Root t(api,api.Call(go_transform,created.Get()));Set(pos_set,t.Get(),position);Set(rot_set,t.Get(),q);
        clones.push_back(std::make_unique<Root>(api,created.Get()));snap.status=u8"Klon önündeki zeminde dik olarak oluşturuldu; AI bileşeni ve ayrı ağ kimliği doğrulandı.";next_scan=0;
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
        if(paused){Clear(false);scene=-1;room_id=player_id=0;gameplay_bridge::Publish(snap);return;}
        if(!initialized){initialized=true;valid=Resolve();}
        if(!valid){snap.status=u8"Oyun API bağlantısı bulunamadı; bu sürümde kontroller kullanılamıyor.";gameplay_bridge::Clear();gameplay_bridge::Publish(snap);return;}
        const bool previously_ready=snap.ready;
        // Root handles may be replaced during discovery. Hooks must never read
        // an old handle while a managed call reenters this update.
        protect=false;ghost_enabled=ammo_enabled=false;protected_player=0;protected_manager=0;protected_attack=protected_state=protected_script=0;
        tool_movement=tool_fall=tool_crossbow=tool_shotgun=tool_inventory=0;
        if(!api.Value<bool>(queue,nullptr,false)){
            gameplay_bridge::Clear();snap.ready=snap.spawn_ready=snap.god_ready=snap.ghost_ready=snap.infinity_ready=false;
            snap.status=u8"Oyun ağ kuyruğu duraklatıldı; kontroller bekliyor.";
            gameplay_bridge::Publish(snap);return;
        }
        Root room(api,api.Call(current_room));int current_scene=0;
        const bool scene_ready=ReadScene(api,get_scene,scene_valid,scene_loaded,current_scene);
        if(!scene_ready){
            Clear(false);scene=-1;room_id=player_id=0;
            snap.status=u8"Oyun sahnesinin yüklenmesi bekleniyor.";
            gameplay_bridge::Publish(snap);return;
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
        snap.authority=authority;snap.god_ready=snap.ready&&(has_modern?(own_modern&&modern_hooks_ready):(own_player&&god_hooks_ready&&own_manager));
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
            if(command.kind==gameplay_bridge::Kind::Spawn){if(spawned_this_frame){gameplay_bridge::Queue(command);continue;}spawned_this_frame=true;Spawn(command.item);continue;}
            if(command.kind==gameplay_bridge::Kind::God){
                snap.god=command.flag&&snap.god_ready;snap.status=snap.god?u8"God Mode açık; kendi karakterinin doğrulanan AI/ölüm girişleri engelleniyor.":u8"God Mode kapalı.";continue;
            }
            if(command.kind==gameplay_bridge::Kind::Ghost){snap.ghost=command.flag&&snap.ghost_ready;snap.status=snap.ghost?u8"Hayalet modu açık; joystick ile duvarlardan geçebilirsin.":u8"Hayalet modu kapalı; normal çarpışma hareketi geri geldi.";continue;}
            if(command.kind==gameplay_bridge::Kind::GhostStep){MoveGhost(command.item>0?.75f:-.75f);continue;}
            if(command.kind==gameplay_bridge::Kind::GhostReturn){if(MoveGhost(0,true))snap.status=u8"Hayalet moduna başladığın konuma döndün.";continue;}
            if(command.kind==gameplay_bridge::Kind::InfinityAmmo){snap.infinity=command.flag&&snap.infinity_ready;snap.status=snap.infinity?u8"Infinity Ammo açık: eldeki Crossbow ve Shotgun için cephane denetimi uygulanır; atış aralığı oyunun normalidir.":u8"Infinity Ammo kapalı.";continue;}
            if(command.kind==gameplay_bridge::Kind::RemoveEnemy){Remove(command.enemy);continue;}
            if(command.kind==gameplay_bridge::Kind::KillEnemy){Kill(command.enemy);continue;}
            if(command.kind==gameplay_bridge::Kind::CloneEnemy){Clone(command.enemy);continue;}
            settings[command.enemy]=command.kind==gameplay_bridge::Kind::ResetEnemy?Enemy{}:command.settings;
            snap.status=authority?u8"Düşman ayarları uygulandı; sıfırlama özgün değerleri geri yükler.":u8"Düşman ayarları bu cihazda uygulandı; diğer oyunculardaki sonuç ağ yetkisine bağlıdır.";
        }
        protect=snap.god&&snap.god_ready;protected_player=player.h;protected_manager=manager.h;protected_scene=scene;protected_room=room_id;
        protected_attack=attacked.h;protected_state=player_state.h;protected_script=script.h;
        tool_movement=movement.h;tool_fall=fall_damage.h;tool_crossbow=crossbow.h;tool_shotgun=shotgun.h;tool_inventory=inventory.h;
        UpdateTools();CollectAmmo();
        if(snap.ready)for(auto& a:actors){Apply(*a);CombatGuard(*a,snap.god&&snap.god_ready);}
        for(size_t k=0;k<kEnemyCount;++k){const auto observed=snap.enemies[k];snap.enemies[k]=settings[k];snap.enemies[k].count=observed.count;snap.enemies[k].death_ready=observed.death_ready;snap.enemies[k].clone_ready=observed.clone_ready;}
        gameplay_bridge::Publish(snap);
    }
};
Runtime* runtime=nullptr;
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
    InstallTools(api);
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
    catch(...){stopped=true;protect=false;ghost_enabled=ammo_enabled=false;gameplay_bridge::Snapshot s;s.status=u8"Oyun özellikleri hata nedeniyle durduruldu.";gameplay_bridge::Clear();gameplay_bridge::Publish(s);}
}
}
