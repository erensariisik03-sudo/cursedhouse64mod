#pragma once
#include "il2cpp_runtime.h"
#include <cstdint>
#include <vector>
namespace lobby_emotes {
enum class State { Delay, MetadataMissing, Loading, NoRoom, NoGarden, ForeignPlayer,
    NoAnimator, AmbiguousAnimator, NoEmotes, Ready, Played, Stopped, SceneChanged, Failed };
struct Binding {
    std::string id,parameter;
    bool operator==(const Binding& other)const{return id==other.id&&parameter==other.parameter;}
};
class Controller {
    const il2cpp::Api& api;
    il2cpp::Class *launcher_class{},*lobby_class{},*emote_class{},*animator_class{},*view_class{},*stream_class{},*string_class{};
    il2cpp::Class *rooms_class{},*lobby_script_class{};
    const il2cpp::Method *find{},*length{},*item{},*alive{},*active{},*room{},*queue{},*connected{},
        *children{},*hierarchy{},*go_transform{},*component_transform{},*is_child{},*instance_id{},
        *is_mine{},*view_id{},*parameters{},*parameter_name{},*parameter_type{},*set_bool{},*get_bool{},*play{},*stop{},*stream_view{};
    il2cpp::Field *mesh{},*menu_avatar{},*bindings{},*emote_animator{},*emote_view{},*stream_animator{},*observed{},*sync_parameters{},*synchronization{};
    const il2cpp::Method *local_player{},*actor_number{},*component_game_object{},*object_tag{},*waiting_tag{};
    il2cpp::Field *owner_actor{},*game_started{};
    bool actor_route=false;
    std::string modern_issue;
    bool started=false,initialized=false,disabled=false,busy=false;
    double start=0,next=0;
    uintptr_t last_room=0;
    int last_avatar=0,last_animator=0;
    int last_actor=0,last_authority=0;
    bool last_actor_route=false;
    uint64_t generation=0;
    std::vector<Binding> last_bindings;
    std::vector<Binding> catalogue;
    std::string metadata_issue;
    std::string target_issue;
    std::string feedback;
    State state=State::Delay;
    bool Resolve();
    bool Alive(il2cpp::Object*);
    bool Below(il2cpp::Object* component,il2cpp::Object* avatar);
    il2cpp::Object* Read(il2cpp::Object*,il2cpp::Field*);
    il2cpp::Object* Read(il2cpp::Object*,const char*);
    il2cpp::Object* Array(il2cpp::Object*,il2cpp::Class*,bool with_inactive=false);
    il2cpp::Object* SceneArray(il2cpp::Class*,bool include_inactive=false);
    il2cpp::Object* RoomsManager();
    il2cpp::Object* ActorAvatar();
    bool ActorOwns(il2cpp::Object* manager,il2cpp::Object* avatar);
    bool NotPreview(il2cpp::Object* script);
    bool RoomTag(il2cpp::Object* avatar);
    il2cpp::Object* FindOne(il2cpp::Class*,bool inactive_launcher=false);
    il2cpp::Object* UniqueChild(il2cpp::Object*,il2cpp::Class*);
    bool Text(il2cpp::Object*,std::string&);
    bool Bindings(il2cpp::Object*,std::vector<Binding>&);
    bool Supports(il2cpp::Object*,const std::vector<Binding>&,std::vector<Binding>&);
    bool Stream(il2cpp::Object*,il2cpp::Object*,il2cpp::Object*,const std::string&);
    bool Current(il2cpp::Object*,il2cpp::Object*,il2cpp::Object*,il2cpp::Object*,il2cpp::Object*);
    void Wait(State);
    void RefreshCatalogue();
    void Report(State);
    void Garden(il2cpp::Object*,il2cpp::Object*,il2cpp::Object*);
public:
    explicit Controller(const il2cpp::Api& a):api(a){}
    void Tick(double now);
    State GetState()const{return state;}
};
void Pump(const il2cpp::Api&);
}
