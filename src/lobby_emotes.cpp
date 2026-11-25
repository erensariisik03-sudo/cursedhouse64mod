#include "lobby_emotes.h"
#include "lobby_emote_bridge.h"
#include <android/log.h>
#include <chrono>
#include <algorithm>
#include <iterator>
#define EMOTE_LOG(...) __android_log_print(ANDROID_LOG_INFO,"multiplayermod",__VA_ARGS__)
namespace lobby_emotes {
namespace {
using namespace il2cpp;
struct Root {
    const Api& api;Handle handle=0;
    Root(const Api& a,Object* v=nullptr):api(a){Reset(v);}
    ~Root(){if(handle)api.gchandle_free(handle);}
    void Reset(Object* v){const Handle next=v?api.gchandle_new(v,false):0;if(handle)api.gchandle_free(handle);handle=next;}
    Object* Get()const{return handle?api.gchandle_get_target(handle):nullptr;}
    Root(const Root&)=delete;
};
const char* Name(State s){switch(s){
    case State::Delay:return "delay";case State::MetadataMissing:return "metadata-missing";
    case State::Loading:return "loading";case State::NoRoom:return "no-room";case State::NoGarden:return "no-garden-mesh";
    case State::ForeignPlayer:return "garden-owner-rejected";case State::NoAnimator:return "no-supported-garden-animator";
    case State::AmbiguousAnimator:return "ambiguous-garden-animator";case State::NoEmotes:return "no-bindings";
    case State::Ready:return "garden-ready";case State::Played:return "garden-played";case State::Stopped:return "garden-stopped";
    case State::SceneChanged:return "scene-changed";case State::Failed:return "playback-not-confirmed";
}return "unknown";}
}
void Controller::Report(State value){if(state!=value){state=value;EMOTE_LOG("v19 garden-panel: %s; peer receipt unverified",Name(value));}}
bool Controller::Resolve(){
    if(!api.object_get_class||!api.class_get_field_from_name||!api.field_get_value||!api.class_get_type||!api.type_get_object||
       !api.string_chars||!api.string_length||!api.string_new||!api.gchandle_new||!api.gchandle_get_target||!api.gchandle_free){metadata_issue="IL2CPP API";return false;}
    auto method=[&](const char* ns,const char* type,const char* name,int n=0){return api.FindMethod(api.Find(ns,type),name,n);};
    launcher_class=api.Find("","launcher");lobby_class=api.Find("","PlayerLobbyManager");emote_class=api.Find("","PlayerEmotesPlayer");
    animator_class=api.Find("UnityEngine","Animator");view_class=api.Find("Photon.Pun","PhotonView");
    stream_class=api.Find("Photon.Pun","PhotonAnimatorView");string_class=api.Find("System","String");
    const std::pair<Class*,const char*> classes[]={{emote_class,"PlayerEmotesPlayer"},
        {animator_class,"Animator"},{view_class,"PhotonView"},{string_class,"String"}};
    for(const auto& value:classes)if(!value.first){metadata_issue=value.second;return false;}
    find=method("UnityEngine","Object","FindObjectsByType",3);
    length=method("System","Array","get_Length");item=method("System","Array","GetValueImpl",1);
    alive=method("UnityEngine","Object","op_Implicit",1);instance_id=method("UnityEngine","Object","GetInstanceID");
    active=method("UnityEngine","Behaviour","get_isActiveAndEnabled");hierarchy=method("UnityEngine","GameObject","get_activeInHierarchy");
    room=method("Photon.Pun","PhotonNetwork","get_CurrentRoom");queue=method("Photon.Pun","PhotonNetwork","get_IsMessageQueueRunning");
    connected=method("Photon.Pun","PhotonNetwork","get_IsConnectedAndReady");
    // Unique six-argument non-generic overload, unlike overloaded GetComponentsInChildren.
    children=method("UnityEngine","GameObject","GetComponentsInternal",6);
    go_transform=method("UnityEngine","GameObject","get_transform");component_transform=method("UnityEngine","Component","get_transform");
    is_child=method("UnityEngine","Transform","IsChildOf",1);
    is_mine=api.FindMethod(view_class,"get_IsMine",0);view_id=api.FindMethod(view_class,"get_ViewID",0);
    parameters=api.FindMethod(animator_class,"get_parameters",0);
    parameter_name=method("UnityEngine","AnimatorControllerParameter","get_name");parameter_type=method("UnityEngine","AnimatorControllerParameter","get_type");
    // These private wrappers have unambiguous managed string + bool ABIs.
    set_bool=api.FindMethod(animator_class,"SetBoolString",2);get_bool=api.FindMethod(animator_class,"GetBoolString",1);
    play=api.FindMethod(emote_class,"Play",1);stop=api.FindMethod(emote_class,"hzb",0);
    mesh=launcher_class?api.class_get_field_from_name(launcher_class,"localPlayerMesh"):nullptr;
    if(lobby_class)menu_avatar=api.class_get_field_from_name(lobby_class,"currentPlayerLobby");
    bindings=api.class_get_field_from_name(emote_class,"emotes");emote_animator=api.class_get_field_from_name(emote_class,"animator");emote_view=api.class_get_field_from_name(emote_class,"pv");
    observed=api.class_get_field_from_name(view_class,"ObservedComponents");synchronization=api.class_get_field_from_name(view_class,"Synchronization");
    if(stream_class){stream_animator=api.class_get_field_from_name(stream_class,"m_Animator");sync_parameters=api.class_get_field_from_name(stream_class,"m_SynchronizeParameters");
        stream_view=api.FindMethod(stream_class,"get_photonView",0);}
    const std::pair<Field*,const char*> fields[]={{bindings,"PlayerEmotesPlayer.emotes"},
        {emote_animator,"PlayerEmotesPlayer.animator"},{emote_view,"PlayerEmotesPlayer.pv"}};
    for(const auto& value:fields)if(!value.first){metadata_issue=value.second;return false;}
    const std::pair<const Method*,const char*> required[]={{find,"Object.FindObjectsByType"},{length,"Array.get_Length"},
        {item,"Array.GetValueImpl"},{alive,"Object.op_Implicit"},{instance_id,"Object.GetInstanceID"},
        {active,"Behaviour.get_isActiveAndEnabled"},{hierarchy,"GameObject.get_activeInHierarchy"},
        {room,"PhotonNetwork.get_CurrentRoom"},{queue,"PhotonNetwork.get_IsMessageQueueRunning"},
        {connected,"PhotonNetwork.get_IsConnectedAndReady"},{children,"GameObject.GetComponentsInternal"},
        {go_transform,"GameObject.get_transform"},{component_transform,"Component.get_transform"},{is_child,"Transform.IsChildOf"},
        {is_mine,"PhotonView.get_IsMine"},{view_id,"PhotonView.get_ViewID"},{parameters,"Animator.get_parameters"},
        {parameter_name,"AnimatorControllerParameter.get_name"},{parameter_type,"AnimatorControllerParameter.get_type"},
        {set_bool,"Animator.SetBoolString"},{get_bool,"Animator.GetBoolString"}};
    for(const auto& value:required)if(!value.first){metadata_issue=value.second;return false;}
    rooms_class=api.Find("","PhotonRoomsManager");lobby_script_class=api.Find("","PlayerLobbyScript");
    local_player=method("Photon.Pun","PhotonNetwork","get_LocalPlayer");actor_number=method("Photon.Realtime","Player","get_ActorNumber");
    component_game_object=method("UnityEngine","Component","get_gameObject");object_tag=method("UnityEngine","GameObject","get_tag");
    waiting_tag=method("<PrivateImplementationDetails>{B6FBF95C-D343-4B5E-9090-977A04841BC0}","a","of");
    owner_actor=lobby_script_class?api.class_get_field_from_name(lobby_script_class,"ownerActorId"):nullptr;
    game_started=rooms_class?api.class_get_field_from_name(rooms_class,"gameStarted"):nullptr;
    if(!rooms_class||!lobby_script_class||!local_player||!actor_number||!component_game_object||!object_tag||!waiting_tag||!owner_actor||!game_started)
        modern_issue=u8"Bekleme karakteri metadata bilgisi eksik.";
    return true;
}
bool Controller::Alive(Object* o){if(!o)return false;void* args[]={o};bool ok=false;auto* v=api.Call(alive,nullptr,args,&ok);return ok&&v&&*static_cast<bool*>(api.object_unbox(v));}
Object* Controller::Read(Object* o,Field* f){Object* v=nullptr;if(o&&f)api.field_get_value(o,f,&v);return v;}
Object* Controller::Read(Object* o,const char* name){return o?Read(o,api.class_get_field_from_name(api.object_get_class(o),name)):nullptr;}
bool Controller::Text(Object* o,std::string& out){
    if(!o||api.object_get_class(o)!=string_class)return false;
    auto* s=reinterpret_cast<String*>(o);const int n=api.string_length(s);const auto* chars=api.string_chars(s);
    if(n<=0||n>128||!chars)return false;
    for(int i=0;i<n;++i)if(chars[i]<32||chars[i]==127)return false;
    out=api.Utf8(s);return !out.empty();
}
Object* Controller::Array(Object* root,Class* klass,bool with_inactive){
    if(!root||!klass)return nullptr;
    const auto* type=api.class_get_type(klass);Root managed_type(api,type?api.type_get_object(type):nullptr);
    if(!managed_type.Get())return nullptr;
    bool search_type=false,recursive=true,include_inactive=with_inactive,reverse=false;
    void* args[]={managed_type.Get(),&search_type,&recursive,&include_inactive,&reverse,nullptr};bool ok=false;
    auto* result=api.Call(children,root,args,&ok);return ok?result:nullptr;
}
Object* Controller::SceneArray(Class* klass,bool include_inactive){
    if(!klass)return nullptr;
    const auto* type=api.class_get_type(klass);Root managed_type(api,type?api.type_get_object(type):nullptr);
    if(!managed_type.Get())return nullptr;
    int inactive=include_inactive?1:0,sort=0;void* args[]={managed_type.Get(),&inactive,&sort};bool ok=false;
    auto* result=api.Call(find,nullptr,args,&ok);return ok?result:nullptr;
}
Object* Controller::RoomsManager(){
    Root array(api,SceneArray(rooms_class,true));const int n=array.Get()?api.Value<int>(length,array.Get(),-1):-1;
    if(n<0||n>16)return nullptr;
    Root result(api);bool ok=false;
    for(int i=0;i<n;++i){void* index[]={&i};Root value(api,api.Call(item,array.Get(),index,&ok));if(!ok)return nullptr;
        if(!Alive(value.Get())||api.object_get_class(value.Get())!=rooms_class)continue;
        if(result.Get())return nullptr;
        result.Reset(value.Get());
    }
    return result.Get();
}
bool Controller::NotPreview(Object* script){
    // Exclude every loaded preview subtree, including disabled menu controllers.
    if(!lobby_class||!menu_avatar)return false;
    Root array(api,SceneArray(lobby_class,true));const int n=array.Get()?api.Value<int>(length,array.Get(),-1):-1;
    if(n<0||n>16)return false;
    bool ok=false;
    for(int i=0;i<n;++i){void* index[]={&i};Root manager(api,api.Call(item,array.Get(),index,&ok));if(!ok)return false;
        if(!Alive(manager.Get()))continue;
        Root preview(api,Read(manager.Get(),menu_avatar));if(Alive(preview.Get())&&Below(script,preview.Get()))return false;
    }
    return true;
}
bool Controller::RoomTag(Object* avatar){
    Root actual(api,api.Call(object_tag,avatar)),expected(api,api.Call(waiting_tag));std::string a,b;
    return Text(actual.Get(),a)&&Text(expected.Get(),b)&&a==b;
}
bool Controller::ActorOwns(Object* manager,Object* avatar){
    if(!modern_issue.empty()||!Alive(manager)||api.object_get_class(manager)!=rooms_class||!Alive(avatar)||!api.Value<bool>(hierarchy,avatar))return false;
    bool started=false;api.field_get_value(manager,game_started,&started);if(started)return false;
    Root player(api,api.Call(local_player));const int actor=player.Get()?api.Value<int>(actor_number,player.Get(),0):0;
    if(actor<=0)return false;
    Root script(api,UniqueChild(avatar,lobby_script_class));if(!Alive(script.Get())||!NotPreview(script.Get())||!RoomTag(avatar))return false;
    int owner=0;api.field_get_value(script.Get(),owner_actor,&owner);
    Root target(api,api.Call(component_game_object,script.Get()));
    return owner==actor&&target.Get()==avatar;
}
Object* Controller::ActorAvatar(){
    target_issue=u8"Bekleme karakteri seçimi tamamlanamadı.";
    Root player(api,api.Call(local_player));const int actor=player.Get()?api.Value<int>(actor_number,player.Get(),0):0;
    if(actor<=0){target_issue=u8"Yerel oyuncu numarası hazır değil.";return nullptr;}
    Root array(api,SceneArray(lobby_script_class));const int n=array.Get()?api.Value<int>(length,array.Get(),-1):-1;
    if(n<0||n>64){target_issue=u8"Bekleme karakterleri okunamadı.";return nullptr;}
    Root result(api);bool ok=false;int own=0,tagged=0,preview=0;
    for(int i=0;i<n;++i){void* index[]={&i};Root script(api,api.Call(item,array.Get(),index,&ok));if(!ok)return nullptr;
        if(!Alive(script.Get())||api.object_get_class(script.Get())!=lobby_script_class||!api.Value<bool>(active,script.Get()))continue;
        int owner=0;api.field_get_value(script.Get(),owner_actor,&owner);if(owner!=actor)continue;++own;
        if(!NotPreview(script.Get())){++preview;continue;}
        Root avatar(api,api.Call(component_game_object,script.Get()));
        if(!Alive(avatar.Get())||!api.Value<bool>(hierarchy,avatar.Get())||!RoomTag(avatar.Get()))continue;
        ++tagged;if(result.Get()){target_issue=u8"Birden fazla yerel bekleme karakteri bulundu.";return nullptr;}
        result.Reset(avatar.Get());
    }
    target_issue=u8"Bekleme modelleri="+std::to_string(n)+u8", benim="+std::to_string(own)+u8", uygun="+std::to_string(tagged)+u8", menü="+std::to_string(preview)+".";
    return result.Get();
}
Object* Controller::FindOne(Class* klass,bool inactive_launcher){
    if(inactive_launcher)target_issue=u8"Oda yöneticisi listesi okunamadı.";
    if(!klass)return nullptr;
    const auto* type=api.class_get_type(klass);Root managed_type(api,type?api.type_get_object(type):nullptr);if(!managed_type.Get())return nullptr;
    int inactive=inactive_launcher?1:0,sort=0;void* args[]={managed_type.Get(),&inactive,&sort};bool ok=false;
    Root array(api,api.Call(find,nullptr,args,&ok));if(!ok||!array.Get())return nullptr;
    const int n=array.Get()?api.Value<int>(length,array.Get(),-1):-1;if(n<0||n>16)return nullptr;Root result(api);
    int live_meshes=0,active_meshes=0;
    for(int i=0;i<n;++i){void* index[]={&i};Root value(api,api.Call(item,array.Get(),index,&ok));if(!ok)return nullptr;
        if(!Alive(value.Get()))continue;
        if(inactive_launcher){
            // Disabled menu controllers may still own the live room mesh. Read
            // their reference without activating UI or invoking their callbacks.
            Root target(api,Read(value.Get(),mesh));
            if(!Alive(target.Get()))continue;
            ++live_meshes;
            if(!api.Value<bool>(hierarchy,target.Get()))continue;
            ++active_meshes;
        }else if(!api.Value<bool>(active,value.Get()))continue;
        if(result.Get()){
            if(inactive_launcher)target_issue=u8"Birden fazla etkin bahçe hedefi bulundu.";
            return nullptr;
        }
        result.Reset(value.Get());}
    if(inactive_launcher){
        target_issue=result.Get()?"":u8"Bahçe hedefi yok: yönetici="+std::to_string(n)+u8", canlı="+std::to_string(live_meshes)+u8", etkin="+std::to_string(active_meshes)+".";
    }
    return result.Get();
}
Object* Controller::UniqueChild(Object* avatar,Class* klass){
    Root array(api,Array(avatar,klass));const int n=array.Get()?api.Value<int>(length,array.Get(),-1):-1;if(n<0||n>64)return nullptr;
    Root result(api);bool ok=false;
    for(int i=0;i<n;++i){void* index[]={&i};Root value(api,api.Call(item,array.Get(),index,&ok));if(!ok)return nullptr;
        if(!Alive(value.Get())||api.object_get_class(value.Get())!=klass||!api.Value<bool>(active,value.Get())||!Below(value.Get(),avatar))continue;
        if(result.Get())return nullptr;
        result.Reset(value.Get());}
    return result.Get();
}
bool Controller::Below(Object* component,Object* avatar){
    Root child(api,api.Call(component_transform,component)),parent(api,api.Call(go_transform,avatar));
    if(!Alive(child.Get())||!Alive(parent.Get()))return false;
    void* args[]={parent.Get()};bool ok=false;auto* result=api.Call(is_child,child.Get(),args,&ok);
    return ok&&result&&*static_cast<bool*>(api.object_unbox(result));
}
bool Controller::Bindings(Object* player,std::vector<Binding>& out){
    Root list(api,Read(player,bindings));auto* klass=list.Get()?api.object_get_class(list.Get()):nullptr;
    auto* count=api.FindMethod(klass,"get_Count",0);auto* at=api.FindMethod(klass,"get_Item",1);
    const int n=api.Value<int>(count,list.Get(),-1);if(!at||n<=0||n>32)return false;
    bool ok=false;
    for(int i=0;i<n;++i){void* index[]={&i};Root entry(api,api.Call(at,list.Get(),index,&ok));if(!ok)return false;if(!entry.Get())continue;
        Root id(api,Read(entry.Get(),"Id")),parameter(api,Read(entry.Get(),"Parameter"));Binding value;
        if(!Text(id.Get(),value.id)||!Text(parameter.Get(),value.parameter))continue;
        auto existing=std::find_if(out.begin(),out.end(),[&](const Binding& b){return b.id==value.id;});
        if(existing!=out.end()){if(existing->parameter!=value.parameter)return false;}else out.push_back(std::move(value));
    }return !out.empty();
}
bool Controller::Supports(Object* animator,const std::vector<Binding>& source,std::vector<Binding>& supported){
    Root array(api,api.Call(parameters,animator));const int n=array.Get()?api.Value<int>(length,array.Get(),-1):-1;if(n<0||n>256)return false;
    std::vector<std::string> bools;bool ok=false;
    for(int i=0;i<n;++i){void* index[]={&i};Root entry(api,api.Call(item,array.Get(),index,&ok));if(!ok)return false;
        if(!entry.Get()||api.Value<int>(parameter_type,entry.Get(),-1)!=4)continue; // actual AnimatorControllerParameterType.Bool
        Root name(api,api.Call(parameter_name,entry.Get()));std::string value;if(Text(name.Get(),value))bools.push_back(std::move(value));}
    for(const auto& b:source)if(std::find(bools.begin(),bools.end(),b.parameter)!=bools.end())supported.push_back(b);
    return !supported.empty();
}
bool Controller::Stream(Object* avatar,Object* animator,Object* pv,const std::string& parameter){
    if(!pv||!stream_class||!stream_animator||!sync_parameters||!stream_view||!observed||!synchronization)return false;
    int mode=0;api.field_get_value(pv,synchronization,&mode);if(mode<=0)return false;
    Root array(api,Array(avatar,stream_class));const int n=array.Get()?api.Value<int>(length,array.Get(),-1):-1;if(n<0||n>64)return false;bool ok=false;
    for(int i=0;i<n;++i){void* index[]={&i};Root view(api,api.Call(item,array.Get(),index,&ok));if(!ok)return false;
        if(!Alive(view.Get())||!api.Value<bool>(active,view.Get())||Read(view.Get(),stream_animator)!=animator||!Below(view.Get(),avatar))continue;
        Root owner(api,api.Call(stream_view,view.Get()));if(owner.Get()!=pv)continue;
        Root observers(api,Read(pv,observed));auto* klass=observers.Get()?api.object_get_class(observers.Get()):nullptr;
        auto* at=api.FindMethod(klass,"get_Item",1);const int count=api.Value<int>(api.FindMethod(klass,"get_Count",0),observers.Get(),-1);
        if(!at||count<0||count>64)return false;
        bool registered=false;
        for(int j=0;j<count;++j){void* pos[]={&j};Root component(api,api.Call(at,observers.Get(),pos,&ok));if(!ok)return false;if(component.Get()==view.Get())registered=true;}
        if(!registered)continue;
        Root list(api,Read(view.Get(),sync_parameters));klass=list.Get()?api.object_get_class(list.Get()):nullptr;
        at=api.FindMethod(klass,"get_Item",1);const int entries=api.Value<int>(api.FindMethod(klass,"get_Count",0),list.Get(),-1);
        if(!at||entries<0||entries>256)return false;
        for(int j=0;j<entries;++j){void* pos[]={&j};Root entry(api,api.Call(at,list.Get(),pos,&ok));if(!ok)return false;if(!entry.Get())continue;
            Root name(api,Read(entry.Get(),"Name"));std::string value;if(!Text(name.Get(),value)||value!=parameter)continue;
            int type=0,sync=0;auto* k=api.object_get_class(entry.Get());auto* t=api.class_get_field_from_name(k,"Type");auto* s=api.class_get_field_from_name(k,"SynchronizeType");
            if(!t||!s)continue;
            api.field_get_value(entry.Get(),t,&type);api.field_get_value(entry.Get(),s,&sync);
            if(type==4&&(sync==1||sync==2))return true;
        }
    }return false; // Read-only. Never append observers or change a peer's serialization layout.
}
bool Controller::Current(Object* current_room,Object* launcher,Object* avatar,Object* animator,Object* pv){
    Root room_now(api,api.Call(room));
    if(room_now.Get()!=current_room||!api.Value<bool>(queue,nullptr,false)||!api.Value<bool>(connected,nullptr,false)||
        !Alive(launcher)||!Alive(avatar)||!api.Value<bool>(hierarchy,avatar)||!Alive(animator)||!api.Value<bool>(active,animator)||!Below(animator,avatar))return false;
    if(pv&&(!Alive(pv)||!Below(pv,avatar)||!api.Value<bool>(is_mine,pv)||api.Value<int>(view_id,pv,0)<=0))return false;
    if(actor_route)return ActorOwns(launcher,avatar);
    return pv&&Read(launcher,mesh)==avatar;
}
void Controller::RefreshCatalogue(){
    Root managers(api,SceneArray(lobby_class,true));const int count=managers.Get()?api.Value<int>(length,managers.Get(),-1):-1;
    if(count<0||count>16)return;
    std::vector<Binding> values;bool ok=false;
    for(int i=0;i<count;++i){int index=i;void* pos[]={&index};Root menu(api,api.Call(item,managers.Get(),pos,&ok));if(!ok)return;
        if(!Alive(menu.Get()))continue;
        Root model(api,Read(menu.Get(),menu_avatar));if(!Alive(model.Get()))continue;
        // A hidden menu still holds serialized bindings. Read them without
        // enabling a component, running Start/Play, or selecting its animator.
        Root players(api,Array(model.Get(),emote_class,true));const int n=players.Get()?api.Value<int>(length,players.Get(),-1):-1;
        if(n<0||n>32)return;
        for(int j=0;j<n;++j){void* at[]={&j};Root source(api,api.Call(item,players.Get(),at,&ok));if(!ok)return;
            if(!Alive(source.Get())||api.object_get_class(source.Get())!=emote_class||!Below(source.Get(),model.Get()))continue;
            std::vector<Binding> candidate;if(!Bindings(source.Get(),candidate))continue;
            if(!values.empty()&&values!=candidate)return; // Do not mix ambiguous catalogues.
            values=std::move(candidate);
        }
    }
    if(!values.empty())catalogue=std::move(values);
}
void Controller::Wait(State reason){
    if(last_room||last_avatar||last_animator)++generation;
    last_room=0;last_avatar=last_animator=last_actor=last_authority=0;last_actor_route=false;last_bindings.clear();feedback.clear();
    if(reason==State::NoRoom||reason==State::Loading||reason==State::MetadataMissing)catalogue.clear();
    std::string message;
    switch(reason){
    case State::Delay:message=u8"Dans hedefi bekleniyor.";break;
    case State::MetadataMissing:message=u8"Dans desteği hazırlanamadı: "+metadata_issue;break;
    case State::Loading:message=u8"Oda yükleniyor. Dans şu an kapalı.";break;
    case State::NoRoom:message=u8"Dans için bir odaya katıl.";break;
    case State::NoGarden:message=target_issue.empty()?u8"Oda açık; bahçe karakteri henüz bulunamadı.":target_issue;break;
    case State::SceneChanged:message=u8"Karakter veya oda değişti. Dans hedefi bekleniyor.";break;
    default:message=u8"Dans hazır değil.";break;
    }
    lobby_emote_bridge::Snapshot snapshot;snapshot.visible=true;snapshot.generation=generation;snapshot.status="v19 | "+message;
    for(const auto& value:catalogue)snapshot.ids.push_back(value.id);
    lobby_emote_bridge::Publish(std::move(snapshot));Report(reason);
}
void Controller::Garden(Object* current_room,Object* launcher,Object* avatar){
    lobby_emote_bridge::Snapshot snapshot;snapshot.visible=true;
    const auto command=lobby_emote_bridge::Take();
    auto finish=[&](State value,const char* message){
        snapshot.status=std::string("v19 | ")+message;snapshot.generation=generation;
        if(!snapshot.ready)for(const auto& entry:catalogue)snapshot.ids.push_back(entry.id);
        Report(value);lobby_emote_bridge::Publish(std::move(snapshot));
    };
    Root pv(api);
    Root views(api,Array(avatar,view_class));const int view_count=views.Get()?api.Value<int>(length,views.Get(),-1):-1;
    bool view_read=true;
    if(view_count==1){int zero=0;void* index[]={&zero};pv.Reset(api.Call(item,views.Get(),index,&view_read));}
    const bool actor_owned=actor_route&&ActorOwns(launcher,avatar);
    const bool live_view=view_read&&Alive(pv.Get())&&api.object_get_class(pv.Get())==view_class&&Below(pv.Get(),avatar);
    const int net_id=live_view?api.Value<int>(view_id,pv.Get(),-1):-1;
    // Object.Instantiate can copy an unregistered ViewID=0 component. Actor/tag
    // ownership still proves this local room replica, but it is not a sender.
    const bool local_stub=actor_owned&&live_view&&net_id==0;
    const bool registered_owned=live_view&&net_id>0&&api.Value<bool>(is_mine,pv.Get())&&(actor_route||api.Value<bool>(active,pv.Get()));
    if(view_count<0||view_count>1||(view_count==1&&!local_stub&&!registered_owned)||
       (actor_route?!actor_owned:!pv.Get())){finish(State::ForeignPlayer,u8"Bahçe karakterinin sahipliği doğrulanamadı.");return;}
    if(local_stub)pv.Reset(nullptr);
    Root player(api,UniqueChild(avatar,emote_class));std::vector<Binding> source_bindings;
    const bool own_bindings=player.Get()&&Bindings(player.Get(),source_bindings);
    if(!own_bindings){
        // V16 animated this menu model by mistake. Here it is only a read-only
        // binding catalogue. No Play, reset, field write or animator call on it.
        source_bindings=catalogue;
        if(source_bindings.empty()){finish(State::NoEmotes,u8"Dans listesi hazır değil; mağazayı açıp tekrar dene.");return;}
    }
    Root animators(api,Array(avatar,animator_class));const int n=animators.Get()?api.Value<int>(length,animators.Get(),-1):-1;
    if(n<0||n>64){finish(State::NoAnimator,u8"Bahçe karakterinin animatörü bulunamadı.");return;}
    Root animator(api);std::vector<Binding> supported;bool ambiguous=false,ok=false;
    for(int i=0;i<n;++i){void* index[]={&i};Root candidate(api,api.Call(item,animators.Get(),index,&ok));
        if(!ok){finish(State::NoAnimator,u8"Bahçe karakteri okunamadı.");return;}
        if(!Alive(candidate.Get())||api.object_get_class(candidate.Get())!=animator_class||!api.Value<bool>(active,candidate.Get())||!Below(candidate.Get(),avatar))continue;
        std::vector<Binding> matches;if(!Supports(candidate.Get(),source_bindings,matches))continue;
        if(matches.size()>supported.size()){animator.Reset(candidate.Get());supported=std::move(matches);ambiguous=false;}
        else if(matches.size()==supported.size())ambiguous=true;
    }
    if(!animator.Get()){finish(State::NoAnimator,u8"Bu bahçe karakterinde uyumlu dans animasyonu bulunamadı.");return;}
    if(ambiguous){finish(State::AmbiguousAnimator,u8"Bahçede birden fazla uygun karakter parçası bulundu.");return;}
    const auto r=reinterpret_cast<uintptr_t>(current_room);const int a=api.Value<int>(instance_id,avatar,0),b=api.Value<int>(instance_id,animator.Get(),0);
    if(!a||!b){finish(State::NoAnimator,u8"Bahçe karakteri doğrulanamadı.");return;}
    Root local(api,actor_route?api.Call(local_player):nullptr);const int actor=local.Get()?api.Value<int>(actor_number,local.Get(),0):0;
    const int authority=api.Value<int>(instance_id,launcher,0);
    if(last_room!=r||last_avatar!=a||last_animator!=b||last_actor!=actor||last_authority!=authority||last_actor_route!=actor_route||last_bindings!=supported){
        last_room=r;last_avatar=a;last_animator=b;last_actor=actor;last_authority=authority;last_actor_route=actor_route;last_bindings=supported;++generation;feedback.clear();}
    snapshot.ready=true;for(const auto& binding:supported)snapshot.ids.push_back(binding.id);
    auto selected=std::find_if(supported.begin(),supported.end(),[&](const Binding& binding){return binding.id==command.id;});
    if(command.pending&&command.generation==generation&&(command.id.empty()||selected!=supported.end())){
        if(!Current(current_room,launcher,avatar,animator.Get(),pv.Get())){Wait(State::SceneChanged);return;}
        Root own_view(api,Read(player.Get(),emote_view));
        const bool native=player.Get()&&own_bindings&&play&&stop&&Read(player.Get(),emote_animator)==animator.Get()&&
            (!own_view.Get()||own_view.Get()==pv.Get());
        bool applied=true;
        if(native){
            Root id(api,command.id.empty()?nullptr:reinterpret_cast<Object*>(api.string_new(command.id.c_str())));
            if(!command.id.empty()&&!id.Get())applied=false;
            else{void* args[]={id.Get()};api.Call(command.id.empty()?stop:play,player.Get(),command.id.empty()?nullptr:args,&applied);}
        }else{
            // Same supported Bool operations as PlayerEmotesPlayer.Play, applied
            // only to the verified garden Animator when that prefab lacks a player.
            for(const auto& binding:supported){
                if(!Current(current_room,launcher,avatar,animator.Get(),pv.Get())){Wait(State::SceneChanged);return;}
                Root parameter(api,reinterpret_cast<Object*>(api.string_new(binding.parameter.c_str())));bool value=false;
                if(!parameter.Get()){applied=false;break;}void* args[]={parameter.Get(),&value};api.Call(set_bool,animator.Get(),args,&applied);if(!applied)break;
            }
            if(applied&&!command.id.empty()){
                if(!Current(current_room,launcher,avatar,animator.Get(),pv.Get())){Wait(State::SceneChanged);return;}
                Root parameter(api,reinterpret_cast<Object*>(api.string_new(selected->parameter.c_str())));bool value=true;
                if(!parameter.Get())applied=false;else{void* args[]={parameter.Get(),&value};api.Call(set_bool,animator.Get(),args,&applied);}
            }
        }
        if(!Current(current_room,launcher,avatar,animator.Get(),pv.Get())){Wait(State::SceneChanged);return;}
        bool confirmed=applied;
        for(const auto& binding:supported){
            Root parameter(api,reinterpret_cast<Object*>(api.string_new(binding.parameter.c_str())));bool read_ok=false;void* args[]={parameter.Get()};
            auto* value=parameter.Get()?api.Call(get_bool,animator.Get(),args,&read_ok):nullptr;
            const bool expected=!command.id.empty()&&binding.parameter==selected->parameter;
            if(!read_ok||!value||*static_cast<bool*>(api.object_unbox(value))!=expected)confirmed=false;
        }
        const bool stream=confirmed&&!command.id.empty()&&Stream(avatar,animator.Get(),pv.Get(),selected->parameter);
        if(!Current(current_room,launcher,avatar,animator.Get(),pv.Get())){Wait(State::SceneChanged);return;}
        feedback=confirmed?(command.id.empty()?u8"Bahçedeki dans durduruldu.":stream?u8"Bahçe komutu uygulandı; diğer cihazdan kontrol et.":u8"Bahçe komutu uygulandı; ağ kanalı doğrulanamadı."):
            u8"Dans komutu işlendi fakat bahçedeki hareket doğrulanamadı.";
        EMOTE_LOG("v19 garden-panel: command=%s route=%s bool-readback=%s existing-stream=%s; peer receipt unverified",
            command.id.empty()?"stop":"play",native?"garden-player":"garden-animator",confirmed?"matched":"failed",stream?"present":"not-confirmed");
        finish(confirmed?(command.id.empty()?State::Stopped:State::Played):State::Failed,feedback.c_str());return;
    }
    finish(State::Ready,feedback.empty()?u8"Bahçe karakteri hazır. Bir dans seç.":feedback.c_str());
}
void Controller::Tick(double now){
    if(disabled||busy)return;
    if(!started){started=true;start=now+3;Wait(State::Delay);return;}
    if(now<start||now<next)return;
    next=now+.5;
    struct Guard{bool& v;explicit Guard(bool& b):v(b){v=true;}~Guard(){v=false;}}guard(busy);
    if(!initialized){initialized=true;if(!Resolve()){disabled=true;Wait(State::MetadataMissing);return;}}
    if(!api.Value<bool>(queue,nullptr,false)){Wait(State::Loading);return;}
    Root current_room(api,api.Call(room));if(!current_room.Get()||!api.Value<bool>(connected,nullptr,false)){Wait(State::NoRoom);return;}
    RefreshCatalogue();
    actor_route=false;Root modern(api,modern_issue.empty()?RoomsManager():nullptr),launcher(api),avatar(api);
    if(modern.Get()){
        bool started=false;api.field_get_value(modern.Get(),game_started,&started);
        if(started){target_issue=u8"Maç başladı; bekleme alanı dansı kapalı.";Wait(State::NoGarden);return;}
        avatar.Reset(ActorAvatar());
        if(avatar.Get()){actor_route=true;launcher.Reset(modern.Get());}
    }
    if(!avatar.Get()){
        const auto modern_status=modern.Get()?target_issue:modern_issue.empty()?u8"Bekleme oda yöneticisi bulunamadı veya belirsiz.":modern_issue;
        launcher.Reset(FindOne(launcher_class,true));avatar.Reset(Read(launcher.Get(),mesh));
        if(!avatar.Get()&&!modern_status.empty())target_issue=modern_status+" / "+target_issue;
    }
    if(!Alive(launcher.Get())||!Alive(avatar.Get())||!api.Value<bool>(hierarchy,avatar.Get())){Wait(State::NoGarden);return;}
    Garden(current_room.Get(),launcher.Get(),avatar.Get());
}
void Pump(const il2cpp::Api& api){
    static Controller* controller=nullptr;static bool stopped=false;if(stopped)return;
    try{if(!controller)controller=new Controller(api);controller->Tick(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    catch(...){stopped=true;lobby_emote_bridge::Publish({true,false,0,{},u8"v19 | Dans modülü durdu. Klavye kullanılabilir."});EMOTE_LOG("v19 garden-panel: native exception; experiment stopped; base features retained");}
}
}
