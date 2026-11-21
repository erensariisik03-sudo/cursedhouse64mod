#include "gameplay.h"
#include "gameplay_bridge.h"
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
const Method* local_player_method=nullptr;
using Hit=void(*)(Object*,Object*,uint8_t,const Method*);
using StateChange=void(*)(Object*,uint8_t,bool,const Method*);
using StateSync=void(*)(Object*,uint8_t,const Method*);
Hit original_bat=nullptr,original_eat=nullptr,original_bite=nullptr,original_bat_rpc=nullptr,original_eat_rpc=nullptr,original_bite_rpc=nullptr;
Unary original_lava=nullptr;
StateChange original_state_change=nullptr;StateSync original_state_sync=nullptr;
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
bool BlockHit(Object* o,Object* target){return ModernProtected(o)&&target&&hook_api->Call(local_player_method)==target;}
#define OWN_HIT(name,original) void name(Object* o,Object* target,uint8_t entity,const Method* m){if(!BlockHit(o,target))original(o,target,entity,m);}
OWN_HIT(Bat,original_bat) OWN_HIT(Eat,original_eat) OWN_HIT(Bite,original_bite)
OWN_HIT(BatRpc,original_bat_rpc) OWN_HIT(EatRpc,original_eat_rpc) OWN_HIT(BiteRpc,original_bite_rpc)
#undef OWN_HIT
void Lava(Object* o,const Method* m){if(!ModernProtected(o))original_lava(o,m);}
void ChangeState(Object* o,uint8_t value,bool sync,const Method* m){if(value!=1||!ModernProtected(o,true))original_state_change(o,value,sync,m);}
void SyncState(Object* o,uint8_t value,const Method* m){if(value!=1||!ModernProtected(o,true))original_state_sync(o,value,m);}
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
#undef MODERN
    modern_hooks_ready=ready;
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
    Actor(const Api& a,Object* p,size_t k):object(a,p),nav(a),animator(a),transform(a),kind(k){}
};
struct Prefab {int id;Root object;std::string name;bool local=false;Prefab(const Api& a,int i,Object* o,std::string n,bool l=false):id(i),object(a,o),name(std::move(n)),local(l){} };
struct Runtime {
    const Api& api;
    bool initialized=false,valid=false,busy=false;
    int scene=-1;uintptr_t room_id=0,player_id=0,modern_id=0,inventory_id=0;uint64_t generation=0;
    double next_scan=0;
    Root player,manager,inventory,attacked,player_state;
    std::array<Class*,kEnemyCount> classes{};
    std::array<Enemy,kEnemyCount> settings{};
    std::vector<std::unique_ptr<Actor>> actors;
    std::vector<std::unique_ptr<Prefab>> prefabs;
    std::vector<std::unique_ptr<Root>> clones;
    gameplay_bridge::Snapshot snap;
    std::string player_issue=u8"Maç karakteri bekleniyor.",inventory_issue=u8"Maç envanteri bekleniyor.";
    const Method *find{},*length{},*item{},*alive{},*active{},*enabled_get{},*enabled_set{},*go{},*go_active{},
        *transform{},*scale_get{},*scale_set{},*nav_get{},*nav_set{},*nav_stop_get{},*nav_stop_set{},*nav_onmesh{},
        *anim_get{},*anim_set{},*current_room{},*master{},*queue{},*get_scene{},*mine{},*name_get{},
        *pos_get{},*rot_get{},*spawn{},*prefix_get{},*path_combine{},*network_destroy{},*local_destroy{},*view_get{},*scene_valid{},*scene_loaded{},
        *children{},*go_transform{},*is_child{},*hand_get{},*hand_pick{},*resources_load{},*local_clone{},*rpc{},*view_id{};
    Class *player_class{},*manager_class{},*inventory_class{},*tie_class{},*attack_class{},*state_class{},*animator_class{},*animation_class{},*view_class{},*shotgun_class{},*pun_class{};
    std::array<Class*,3> clone_managers{};
    explicit Runtime(const Api& a):api(a),player(a),manager(a),inventory(a),attacked(a),player_state(a){}
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
    const Method* ExactClone(){
        if(!api.class_get_methods||!api.method_get_param_count||!api.method_get_param)return nullptr;
        auto* c=api.Find("UnityEngine","Object");if(!c)return nullptr;
        void* it=nullptr;const Method* found=nullptr;const char* types[]={"Object","Vector3","Quaternion"};
        for(int n=0;n<1024;++n){auto* m=api.class_get_methods(c,&it);if(!m)break;
            if(std::strcmp(api.method_get_name(m),"Instantiate")||api.method_get_param_count(m)!=3)continue;
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
        name_get=ResolveMethod("UnityEngine","Object","get_name");pos_get=ResolveMethod("UnityEngine","Transform","get_position");rot_get=ResolveMethod("UnityEngine","Transform","get_rotation");
        spawn=ResolveMethod("Photon.Pun","PhotonNetwork","Instantiate",5);path_combine=ResolveMethod("System.IO","Path","Combine",2);
        prefix_get=ResolveMethod("<PrivateImplementationDetails>{B6FBF95C-D343-4B5E-9090-977A04841BC0}","a","bvx");
        network_destroy=ExactDestroy();local_destroy=ResolveMethod("UnityEngine","Object","Destroy",1);
        player_class=api.Find("","playerController");manager_class=api.Find("","playerManager");inventory_class=api.Find("","HandlePickUpItemInteractions");tie_class=api.Find("","HandItemTie");
        attack_class=api.Find("","HandleEnemyAttacked");state_class=api.Find("","PlayerStateManager");shotgun_class=api.Find("","ShotgunManager");
        animator_class=api.Find("UnityEngine","Animator");animation_class=api.Find("UnityEngine","Animation");view_class=api.Find("Photon.Pun","PhotonView");
        pun_class=api.Find("Photon.Pun","MonoBehaviourPun");
        clone_managers={api.Find("","GrannyManager"),api.Find("","GrandpaManager"),api.Find("","SpiderMomManager")};
        for(size_t k=0;k<kEnemyCount;++k)classes[k]=api.Find("",specs[k].klass);
        const Method* required[]={find,length,item,alive,active,enabled_get,enabled_set,go,go_active,transform,scale_get,scale_set,current_room,master,queue,get_scene,scene_valid,scene_loaded,mine};
        return (player_class||attack_class)&&std::all_of(std::begin(required),std::end(required),[](auto* m){return m!=nullptr;});
    }
    void Clear(bool restore){
        protect=false;protected_player=0;protected_manager=0;protected_attack=protected_state=0;
        if(restore)for(auto& actor:actors)Restore(*actor);
        actors.clear();prefabs.clear();clones.clear();player.Set(nullptr);manager.Set(nullptr);inventory.Set(nullptr);attacked.Set(nullptr);player_state.Set(nullptr);
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
        for(const auto& p:prefabs)snap.items.push_back({p->id,p->id==48?u8"Pompalı mermisi (önüme)":p->name});
    }
    Object* PathFor(const std::string& value){
        bool ok=false;Root prefix(api,api.Call(prefix_get,nullptr,nullptr,&ok));if(!ok||!prefix.Get())return nullptr;
        Root name(api,reinterpret_cast<Object*>(api.string_new(value.c_str())));if(!name.Get())return nullptr;
        void* args[]={prefix.Get(),name.Get()};return api.Call(path_combine,nullptr,args);
    }
    void AddAmmo(){
        for(auto& p:prefabs)if(p->id==48){bool has_view=false;if(Children(p->object.Get(),view_class,[&](Object*){has_view=true;}))p->local=!has_view;return;}
        Root linked(api,Ref(inventory.Get(),"shotgunManager")),source(api,Ref(linked.Get(),"moreAmmo"));bool ambiguous=false;
        if(!Alive(source.Get()))Each(shotgun_class,[&](Object* o){Root ammo(api,Ref(o,"moreAmmo"));if(!Alive(ammo.Get()))return;if(source.Get()&&source.Get()!=ammo.Get())ambiguous=true;source.Set(ammo.Get());});
        if(!source.Get()||ambiguous||!local_clone)return;
        Root text(api,api.Call(name_get,source.Get()));if(!text.Get())return;
        std::string name=api.Utf8(reinterpret_cast<String*>(text.Get()));if(name.empty()||name.size()>160)return;
        const std::string suffix="(Clone)";if(name.size()>suffix.size()&&name.compare(name.size()-suffix.size(),suffix.size(),suffix)==0)name.resize(name.size()-suffix.size());
        Root asset(api);
        if(resources_load&&prefix_get&&path_combine){Root path(api,PathFor(name));auto* type=api.Find("UnityEngine","GameObject");Root reflection(api,type?api.type_get_object(api.class_get_type(type)):nullptr);
            if(path.Get()&&reflection.Get()){void* args[]={path.Get(),reflection.Get()};asset.Set(api.Call(resources_load,nullptr,args));}
        }
        bool has_view=false;
        Object* target=asset.Get()?asset.Get():source.Get();
        if(!Children(target,view_class,[&](Object*){has_view=true;}))return;
        // Only SDK instantiation of a real asset may allocate a network ID.
        // A scene object with a PhotonView must never be copied with Unity.Instantiate.
        if(has_view&&!asset.Get())return;
        prefabs.push_back(std::make_unique<Prefab>(api,48,target,name,!has_view));
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
        if(it==prefabs.end()||!InHierarchy(inventory.Get())||!Owned(inventory.Get(),"pv")){snap.status=u8"Eşya seçimi bu oyuncu oturumuna ait değil.";return;}
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
        Root created(api,Create(**it,position,rotation,ok));
        snap.status=ok&&Alive(created.Get())?((*it)->local?u8"Pompalı mermisi önünde yerel olarak oluşturuldu.":u8"Eşya önünde oluşturuldu; normal ağ oluşturma yolu kullanıldı."):u8"Oyun bu prefabı oluşturamadı; konum veya kaynak kaydı hazır olmayabilir.";
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
        const char* fields[]={"grannyPrefab","grandpaPrefab","spiderMomPrefab"};const char* points[]={"grannyStartTrans","grandpaStartTrans","momSpawnTrans"};
        Root prefab(api),point(api);bool ambiguous=false;
        Each(clone_managers[kind],[&](Object* o){if(!InHierarchy(o))return;Root value(api,Ref(o,fields[kind]));if(!Alive(value.Get()))return;if(prefab.Get()&&prefab.Get()!=value.Get())ambiguous=true;prefab.Set(value.Get());point.Set(Ref(o,points[kind]));});
        if(!prefab.Get()||ambiguous){snap.status=u8"Klon prefabı bulunamadı veya belirsiz.";return;}
        if(InHierarchy(inventory.Get())&&Owned(inventory.Get(),"pv"))point.Set(Ref(inventory.Get(),"dropPointTransform"));
        if(!Alive(point.Get())){snap.status=u8"Klon konumu hazır değil.";return;}
        Root text(api,api.Call(name_get,prefab.Get()));if(!text.Get())return;
        const auto name=api.Utf8(reinterpret_cast<String*>(text.Get()));
        if(name.empty()||name.size()>160)return;
        bool pose_ok=false;Root position_box(api,api.Call(pos_get,point.Get(),nullptr,&pose_ok));if(!pose_ok||!position_box.Get())return;
        const Vec3 p=*static_cast<Vec3*>(api.object_unbox(position_box.Get()));
        Root rotation_box(api,api.Call(rot_get,point.Get(),nullptr,&pose_ok));if(!pose_ok||!rotation_box.Get())return;
        const Quat q=*static_cast<Quat*>(api.object_unbox(rotation_box.Get()));
        Vec3 position{p.x+1.5f+static_cast<float>(clones.size())*.4f,p.y,p.z};
        if(!Finite(position)||!std::isfinite(q.x)||!std::isfinite(q.y)||!std::isfinite(q.z)||!std::isfinite(q.w))return;
        Prefab source(api,0,prefab.Get(),name);bool ok=false;Root created(api,Create(source,position,q,ok));
        if(ok&&Alive(created.Get())){clones.push_back(std::make_unique<Root>(api,created.Get()));snap.status=u8"Yeni ağ kimliğiyle düşman klonu oluşturuldu. Ölüm isteği oyunun ana düşmanını yönetir.";next_scan=0;}
        else snap.status=u8"Oyun klon prefabını oluşturmayı reddetti.";
    }
    void Capabilities(){
        for(size_t k=0;k<kEnemyCount;++k){snap.enemies[k].death_ready=false;snap.enemies[k].clone_ready=false;}
        const char* fields[]={"grannyPrefab","grandpaPrefab","spiderMomPrefab"};const char* deaths[]={"RPC_SyncShotShotgunGranny","RPC_SyncShotShotgunGrandpa"};
        for(size_t k=0;k<3;++k)Each(clone_managers[k],[&](Object* o){if(!InHierarchy(o))return;
            Root prefab(api,Ref(o,fields[k]));if(Alive(prefab.Get())&&spawn&&prefix_get&&path_combine)snap.enemies[k].clone_ready=true;
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
        protect=false;protected_player=0;protected_manager=0;protected_attack=protected_state=0;
        if(!api.Value<bool>(queue,nullptr,false)){
            gameplay_bridge::Clear();snap.ready=snap.spawn_ready=snap.god_ready=false;
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
            if(modern_id!=attack_id||inventory_id!=inv_id){for(auto& a:actors)Restore(*a);settings={};snap.god=false;gameplay_bridge::Clear();++generation;modern_id=attack_id;inventory_id=inv_id;}
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
        const bool has_modern=attacked.Get()||player_state.Get();
        snap.authority=authority;snap.god_ready=snap.ready&&(has_modern?(own_modern&&modern_hooks_ready):(own_player&&god_hooks_ready&&own_manager));
        const bool spawn_api=spawn&&prefix_get&&path_combine&&pos_get&&rot_get&&name_get;
        snap.spawn_ready=snap.ready&&own_inventory&&spawn_api&&!prefabs.empty();
        snap.spawn_status=!snap.ready?u8"Bir oyun odasına gir.":!own_inventory?(inventory_issue.empty()?u8"Envanter sahipliği yeniden doğrulanıyor.":inventory_issue):
            !spawn_api?u8"Bu sürümün eşya oluşturma bağlantısı eksik.":prefabs.empty()?u8"Oyunun eşya prefab listesini yüklemesi bekleniyor.":u8"Eşya oluşturma hazır.";
        snap.god_status=!snap.ready?u8"Bir oyun odasına gir.":own_modern?(modern_hooks_ready?u8"Güncel oyuncu ve ölüm koruması hazır.":u8"Güncel oyuncu bulundu; ölüm koruması bağlantıları eksik."):(attacked.Get()||player_state.Get())?u8"Güncel karakterin saldırı ve durum bileşenlerinin sahipliği bekleniyor.":!own_player?(player_issue.empty()?u8"Oyuncu sahipliği yeniden doğrulanıyor.":player_issue):
            !own_manager?u8"Kendi karakterinin oyun yöneticisi bekleniyor.":!god_hooks_ready?u8"Ölüm koruması bağlantılarının tamamı kurulamadı.":u8"Kendi karakterin için God Mode hazır.";
        if(!snap.god_ready)snap.god=false;
        if(!snap.ready){protect=false;gameplay_bridge::Clear();snap.status=u8"Oyun odasına girmen bekleniyor.";}
        else if(!previously_ready||snap.status==u8"Oyun sahnesi bekleniyor.")snap.status=u8"Oda ve sahne hazır. Her kontrol kendi hedefi bulunduğunda açılır.";
        bool spawned_this_frame=false;
        for(const auto& command:gameplay_bridge::Take()){
            if(!snap.ready||command.context!=generation||now-command.queued>3)continue;
            if(command.kind==gameplay_bridge::Kind::Spawn){if(spawned_this_frame){gameplay_bridge::Queue(command);continue;}spawned_this_frame=true;Spawn(command.item);continue;}
            if(command.kind==gameplay_bridge::Kind::God){
                snap.god=command.flag&&snap.god_ready;snap.status=snap.god?u8"God Mode açık; kendi karakterinin doğrulanan AI/ölüm girişleri engelleniyor.":u8"God Mode kapalı.";continue;
            }
            if(command.kind==gameplay_bridge::Kind::RemoveEnemy){Remove(command.enemy);continue;}
            if(command.kind==gameplay_bridge::Kind::KillEnemy){Kill(command.enemy);continue;}
            if(command.kind==gameplay_bridge::Kind::CloneEnemy){Clone(command.enemy);continue;}
            settings[command.enemy]=command.kind==gameplay_bridge::Kind::ResetEnemy?Enemy{}:command.settings;
            snap.status=authority?u8"Düşman ayarları uygulandı; sıfırlama özgün değerleri geri yükler.":u8"Düşman ayarları bu cihazda uygulandı; diğer oyunculardaki sonuç ağ yetkisine bağlıdır.";
        }
        protect=snap.god&&snap.god_ready;protected_player=player.h;protected_manager=manager.h;protected_scene=scene;protected_room=room_id;
        protected_attack=attacked.h;protected_state=player_state.h;
        if(snap.ready)for(auto& a:actors)Apply(*a);
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
    catch(...){stopped=true;protect=false;gameplay_bridge::Snapshot s;s.status=u8"Oyun özellikleri hata nedeniyle durduruldu.";gameplay_bridge::Clear();gameplay_bridge::Publish(s);}
}
}
