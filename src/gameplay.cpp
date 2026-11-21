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
struct Actor {
    Root object,nav,animator,transform;size_t kind;
    std::vector<Scalar> speed_fields,animation_fields;
    float nav_base=0,nav_last=0,anim_base=1,anim_last=1;
    bool nav_changed=false,anim_changed=false,enabled_changed=false,enabled_before=true;
    bool stop_changed=false,stop_before=false,scale_changed=false;
    Vec3 scale_before{},scale_last{};
    Actor(const Api& a,Object* p,size_t k):object(a,p),nav(a),animator(a),transform(a),kind(k){}
};
struct Prefab {int id;Root object;std::string name;Prefab(const Api& a,int i,Object* o,std::string n):id(i),object(a,o),name(std::move(n)){} };
struct Runtime {
    const Api& api;
    bool initialized=false,valid=false,was_authority=false,busy=false;
    int scene=-1;uintptr_t room_id=0,player_id=0;uint64_t generation=0;
    double next_scan=0,next_spawn=0;
    Root player,manager,inventory;
    std::array<Class*,kEnemyCount> classes{};
    std::array<Enemy,kEnemyCount> settings{};
    std::vector<std::unique_ptr<Actor>> actors;
    std::vector<std::unique_ptr<Prefab>> prefabs;
    gameplay_bridge::Snapshot snap;
    std::string player_issue=u8"Maç karakteri bekleniyor.",inventory_issue=u8"Maç envanteri bekleniyor.";
    const Method *find{},*length{},*item{},*alive{},*active{},*enabled_get{},*enabled_set{},*go{},*go_active{},
        *transform{},*scale_get{},*scale_set{},*nav_get{},*nav_set{},*nav_stop_get{},*nav_stop_set{},*nav_onmesh{},
        *anim_get{},*anim_set{},*current_room{},*master{},*queue{},*get_scene{},*mine{},*name_get{},
        *pos_get{},*rot_get{},*spawn{},*prefix_get{},*path_combine{},*network_destroy{},*local_destroy{},*view_get{},*scene_valid{},*scene_loaded{};
    Class *player_class{},*manager_class{},*inventory_class{},*tie_class{};
    explicit Runtime(const Api& a):api(a),player(a),manager(a),inventory(a){}
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
        if(!pv.Get()&&view_get)pv.Set(api.Call(view_get,o));
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
        name_get=ResolveMethod("UnityEngine","Object","get_name");pos_get=ResolveMethod("UnityEngine","Transform","get_position");rot_get=ResolveMethod("UnityEngine","Transform","get_rotation");
        spawn=ResolveMethod("Photon.Pun","PhotonNetwork","Instantiate",5);path_combine=ResolveMethod("System.IO","Path","Combine",2);
        prefix_get=ResolveMethod("<PrivateImplementationDetails>{B6FBF95C-D343-4B5E-9090-977A04841BC0}","a","bvx");
        network_destroy=ExactDestroy();local_destroy=ResolveMethod("UnityEngine","Object","Destroy",1);
        player_class=api.Find("","playerController");manager_class=api.Find("","playerManager");inventory_class=api.Find("","HandlePickUpItemInteractions");tie_class=api.Find("","HandItemTie");
        for(size_t k=0;k<kEnemyCount;++k)classes[k]=api.Find("",specs[k].klass);
        const Method* required[]={find,length,item,alive,active,enabled_get,enabled_set,go,go_active,transform,scale_get,scale_set,current_room,master,queue,get_scene,scene_valid,scene_loaded,mine};
        return player_class&&std::all_of(std::begin(required),std::end(required),[](auto* m){return m!=nullptr;});
    }
    void Clear(bool restore){
        protect=false;protected_player=0;protected_manager=0;
        if(restore)for(auto& actor:actors)Restore(*actor);
        actors.clear();prefabs.clear();player.Set(nullptr);manager.Set(nullptr);inventory.Set(nullptr);
        settings={};snap={};next_scan=0;gameplay_bridge::Clear();
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
        if(specs[a.kind].pv&&!Owned(a.object.Get(),specs[a.kind].pv)){Restore(a);return;}
        ScalarApply(a,a.speed_fields,cfg.speed);ScalarApply(a,a.animation_fields,cfg.animation);
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
        if(Alive(a.transform.Get())){
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
                if(specs[k].pv&&!Owned(o,specs[k].pv))return;
                ++snap.enemies[k].count;
                if(actors.size()>=48)return;
                if(std::any_of(actors.begin(),actors.end(),[&](const auto& a){return a->object.Get()==o;}))return;
                auto actor=std::make_unique<Actor>(api,o,k);
                actor->nav.Set(Ref(o,specs[k].nav));actor->animator.Set(Ref(o,specs[k].animator));actor->transform.Set(api.Call(transform,o));
                FloatFields(*actor,specs[k].speed,actor->speed_fields);FloatFields(*actor,specs[k].animation,actor->animation_fields);
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
        for(const auto& p:prefabs)snap.items.push_back({p->id,p->name});
    }
    void Spawn(int id,double now){
        if(!snap.spawn_ready||now<next_spawn){snap.status=u8"Eşya oluşturma hazır değil veya bir saniyelik bekleme sürüyor.";return;}
        const auto it=std::find_if(prefabs.begin(),prefabs.end(),[&](const auto& p){return p->id==id;});
        if(it==prefabs.end()||!InHierarchy(inventory.Get())||!Owned(inventory.Get(),"pv")){snap.status=u8"Eşya seçimi bu oyuncu oturumuna ait değil.";return;}
        Root point(api,Ref(inventory.Get(),"dropPointTransform"));if(!Alive(point.Get())||!Alive((*it)->object.Get()))return;
        bool ok=false;Root prefix(api,api.Call(prefix_get,nullptr,nullptr,&ok));if(!ok||!prefix.Get())return;
        Root name(api,reinterpret_cast<Object*>(api.string_new((*it)->name.c_str())));if(!name.Get())return;
        void* combine[]={prefix.Get(),name.Get()};Root path(api,api.Call(path_combine,nullptr,combine,&ok));if(!ok||!path.Get())return;
        Root pos(api,api.Call(pos_get,point.Get(),nullptr,&ok));if(!ok||!pos.Get())return;const Vec3 position=*static_cast<Vec3*>(api.object_unbox(pos.Get()));
        Root rot(api,api.Call(rot_get,point.Get(),nullptr,&ok));if(!ok||!rot.Get())return;const Quat rotation=*static_cast<Quat*>(api.object_unbox(rot.Get()));
        if(!Finite(position)||!std::isfinite(rotation.x)||!std::isfinite(rotation.y)||!std::isfinite(rotation.z)||!std::isfinite(rotation.w))return;
        uint8_t group=0;Vec3 p=position;Quat q=rotation;void* args[]={path.Get(),&p,&q,&group,nullptr};
        next_spawn=now+1;Root created(api,api.Call(spawn,nullptr,args,&ok));
        snap.status=ok&&Alive(created.Get())?u8"Eşya önünde oluşturuldu; oyunun normal ağ oluşturma yolu kullanıldı.":u8"Oyun bu prefabı oluşturamadı; konum veya kaynak kaydı hazır olmayabilir.";
    }
    void Remove(size_t kind){
        if(!snap.authority){snap.status=u8"Düşman yönetimi için oda yöneticisi olmalısın.";return;}
        int removed=0;
        for(auto& a:actors){
            if(removed>=8)break;
            if(a->kind!=kind||!Alive(a->object.Get()))continue;
            Root target(api,nullptr);
            if(specs[kind].pv){
                if(!Owned(a->object.Get(),specs[kind].pv)||!network_destroy)continue;
                Root pv(api,Ref(a->object.Get(),specs[kind].pv));target.Set(api.Call(go,pv.Get()));
            }else target.Set(api.Call(go,a->object.Get()));
            if(!Alive(target.Get()))continue;
            Restore(*a);
            if(!Alive(target.Get())||(specs[kind].pv&&!Owned(a->object.Get(),specs[kind].pv)))continue;
            void* args[]={target.Get()};bool ok=false;
            api.Call(specs[kind].pv?network_destroy:local_destroy,nullptr,args,&ok);if(ok)++removed;
        }
        settings[kind]={};
        snap.status=removed?(kind>=4?u8"Yerel kaldırma çağrısı yapıldı; diğer istemcilerde görünümü değişmeyebilir.":u8"Sahibi olduğun düşman için ağdan kaldırma çağrısı yapıldı. Sonucu sahnede kontrol et; ölüm/ödül akışı çalıştırılmaz."):u8"Kaldırılabilecek, sahipliği doğrulanmış düşman bulunamadı.";
        next_scan=0;
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
        protect=false;protected_player=0;protected_manager=0;
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
            if(room.Get())Discover();
            next_scan=now+1;
        }
        const bool authority=room.Get()&&api.Value<bool>(master,nullptr,false);
        if(was_authority&&!authority){for(auto& a:actors)Restore(*a);settings={};}
        // Enemy controls need the room/scene and owned enemy, not a player
        // controller. Spawn and God Mode keep their own stricter requirements.
        was_authority=authority;snap.context=generation;snap.ready=room.Get()!=nullptr;
        const bool own_player=InHierarchy(player.Get())&&Owned(player.Get(),"qsa");
        const bool own_inventory=InHierarchy(inventory.Get())&&Owned(inventory.Get(),"pv");
        const bool own_manager=InHierarchy(manager.Get())&&Owned(manager.Get(),"raf");
        snap.authority=authority;snap.god_ready=snap.ready&&own_player&&god_hooks_ready&&own_manager;
        const bool spawn_api=spawn&&prefix_get&&path_combine&&pos_get&&rot_get&&name_get;
        snap.spawn_ready=snap.ready&&own_inventory&&spawn_api&&!prefabs.empty();
        snap.spawn_status=!snap.ready?u8"Bir oyun odasına gir.":!own_inventory?(inventory_issue.empty()?u8"Envanter sahipliği yeniden doğrulanıyor.":inventory_issue):
            !spawn_api?u8"Bu sürümün eşya oluşturma bağlantısı eksik.":prefabs.empty()?u8"Oyunun eşya prefab listesini yüklemesi bekleniyor.":u8"Eşya oluşturma hazır.";
        snap.god_status=!snap.ready?u8"Bir oyun odasına gir.":!own_player?(player_issue.empty()?u8"Oyuncu sahipliği yeniden doğrulanıyor.":player_issue):
            !own_manager?u8"Kendi karakterinin oyun yöneticisi bekleniyor.":!god_hooks_ready?u8"Ölüm koruması bağlantılarının tamamı kurulamadı.":u8"Kendi karakterin için God Mode hazır.";
        if(!snap.god_ready)snap.god=false;
        if(!snap.ready){protect=false;gameplay_bridge::Clear();snap.status=u8"Oyun odasına girmen bekleniyor.";}
        else if(!previously_ready||snap.status==u8"Oyun sahnesi bekleniyor.")snap.status=u8"Oda ve sahne hazır. Her kontrol kendi hedefi bulunduğunda açılır.";
        for(const auto& command:gameplay_bridge::Take()){
            if(!snap.ready||command.context!=generation||now-command.queued>3)continue;
            if(command.kind==gameplay_bridge::Kind::Spawn){Spawn(command.item,now);continue;}
            if(command.kind==gameplay_bridge::Kind::God){
                snap.god=command.flag&&snap.god_ready;snap.status=snap.god?u8"God Mode açık; kendi karakterinin doğrulanan AI/ölüm girişleri engelleniyor.":u8"God Mode kapalı.";continue;
            }
            if(!authority){snap.status=u8"Düşman yönetimi için oda yöneticisi olmalısın.";continue;}
            if(command.kind==gameplay_bridge::Kind::RemoveEnemy){Remove(command.enemy);continue;}
            settings[command.enemy]=command.kind==gameplay_bridge::Kind::ResetEnemy?Enemy{}:command.settings;
            snap.status=u8"Düşman ayarları uygulandı; sıfırlama özgün değerleri geri yükler.";
        }
        protect=snap.god&&snap.god_ready;protected_player=player.h;protected_manager=manager.h;protected_scene=scene;protected_room=room_id;
        if(authority&&snap.ready)for(auto& a:actors)Apply(*a);
        for(size_t k=0;k<kEnemyCount;++k){const int count=snap.enemies[k].count;snap.enemies[k]=settings[k];snap.enemies[k].count=count;}
        gameplay_bridge::Publish(snap);
    }
};
Runtime* runtime=nullptr;
}
void Install(const Api& api){
    if(hooks_installed)return;
    hooks_installed=true;hook_api=&api;unity_thread=std::this_thread::get_id();
    if(!api.domain_get||!api.class_get_field_from_name||!api.field_get_value||!api.gchandle_new)return;
    auto* player=api.Find("","playerController");auto* manager=api.Find("","playerManager");if(!player||!manager)return;
    scene_method=api.FindMethod(api.Find("UnityEngine.SceneManagement","SceneManager"),"GetActiveScene",0);if(!scene_method)return;
    auto* scene_class=api.Find("UnityEngine.SceneManagement","Scene");
    scene_valid_method=api.FindMethod(scene_class,"IsValidInternal",1);scene_loaded_method=api.FindMethod(scene_class,"GetIsLoadedInternal",1);
    room_method=api.FindMethod(api.Find("Photon.Pun","PhotonNetwork"),"get_CurrentRoom",0);
    mine_method=api.FindMethod(api.Find("Photon.Pun","PhotonView"),"get_IsMine",0);
    view_method=api.FindMethod(api.Find("Photon.Pun","MonoBehaviourPun"),"get_photonView",0);
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
