#include "lobby_emotes.h"
#include "lobby_emote_bridge.h"
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
namespace il2cpp {
struct Method {std::string owner,name;};struct Field {std::string name;bool scalar=false;};
struct Class {std::string ns,name;Class* parent=nullptr;std::map<std::pair<std::string,int>,Method> methods;std::map<std::string,Field> fields;};
struct Type {Class* klass;};
struct Object {Class* klass;int instance=0;bool alive=true,active=true,flag=false;int number=0;Class* reflection=nullptr;
 std::u16string text;Object* parent=nullptr;std::map<std::string,Object*> refs;std::map<std::string,int> ints;
 std::vector<Object*> items;std::map<Class*,std::vector<Object*>> children;std::map<std::string,bool> bools;};
struct String:Object{};struct Domain{};struct Assembly{};struct Image{};
}
using namespace il2cpp;
namespace {
struct Fake;Fake* f;
struct Fake {
 Domain domain;Assembly assembly;Image image;const Assembly* assemblies[1]={&assembly};
 std::vector<std::unique_ptr<Class>> classes;std::vector<std::unique_ptr<Object>> objects;
 std::map<Class*,Type> types;std::map<Handle,Object*> roots;Handle next=0;int ids=0;
 Class *boolean{},*integer{},*array{},*list{},*reflection{},*string{},*go{},*transform{},*emote{},*animator{},*view{},*stream{},*binding{},*parameter{},*sync{};
 Object *modern{},*actor_script{},*local_actor{};Class* script_class{};std::vector<Object*> scripts;
 Object *room{},*launcher{},*lobby{},*garden{},*menu{},*garden_anim{},*menu_anim{},*preview{},*native{},*pv{},*source_bindings{},*stream_component{};
 bool queue=true,connected=true,own=true,duplicate_launcher=false,fail_children=false,fail_parameters=false,fail_play=false,ignore_play=false;
 bool change_owner_on_set=false;
 bool fail_set=false,leave_on_set=false,leave_on_play=false,leave_on_readback=false,fail_allocate=false;
 int calls=0,lookups=0,sets=0,plays=0,stops=0;
 lobby_emotes::Controller* reenter=nullptr;
 Class* C(const char* ns,const char* name,Class* parent=nullptr){auto v=std::make_unique<Class>();v->ns=ns;v->name=name;v->parent=parent;auto* p=v.get();classes.push_back(std::move(v));return p;}
 Object* O(Class* c){auto v=std::make_unique<Object>();v->klass=c;v->instance=++ids;auto* p=v.get();objects.push_back(std::move(v));return p;}
 Object* S(const std::string& text){auto* o=O(string);for(char c:text)o->text+=c;return o;}
 Object* B(bool value){auto* o=O(boolean);o->flag=value;return o;}
 Object* I(int value){auto* o=O(integer);o->number=value;return o;}
 void M(Class* c,const char* name,int n=0){c->methods[{name,n}]={c->name,name};}
 void F(Class* c,const char* name,bool scalar=false){c->fields[name]={name,scalar};}
 Object* Transform(Object* o,Object* parent=nullptr){auto* t=O(transform);o->refs["transform"]=t;t->parent=parent?parent->refs.at("transform"):nullptr;return t;}
 Object* Params(){auto* a=O(array);for(auto name:{"danceParamA","danceParamB"}){auto* p=O(parameter);p->refs["name"]=S(name);p->number=4;a->items.push_back(p);}return a;}
 Fake(){
  f=this;lobby_emote_bridge::Publish({});
  boolean=C("System","Boolean");integer=C("System","Int32");array=C("System","Array");list=C("System.Collections.Generic","List");reflection=C("System","Type");string=C("System","String");
  auto* obj=C("UnityEngine","Object");auto* component=C("UnityEngine","Component",obj);auto* behaviour=C("UnityEngine","Behaviour",component);
  go=C("UnityEngine","GameObject",obj);transform=C("UnityEngine","Transform",component);emote=C("","PlayerEmotesPlayer",behaviour);
  animator=C("UnityEngine","Animator",behaviour);view=C("Photon.Pun","PhotonView",behaviour);
  auto* pun=C("Photon.Pun","MonoBehaviourPun",behaviour);stream=C("Photon.Pun","PhotonAnimatorView",pun);
  auto* l=C("","launcher",behaviour);auto* lm=C("","PlayerLobbyManager",behaviour);auto* pn=C("Photon.Pun","PhotonNetwork");
  binding=C("","EmoteBinding");parameter=C("UnityEngine","AnimatorControllerParameter");sync=C("","SynchronizedParameter");
  M(obj,"FindObjectsByType",3);M(obj,"op_Implicit",1);M(obj,"GetInstanceID");M(behaviour,"get_isActiveAndEnabled");
  M(go,"get_activeInHierarchy");M(go,"GetComponentsInternal",6);M(go,"get_transform");M(component,"get_transform");M(transform,"IsChildOf",1);
  M(array,"get_Length");M(array,"GetValueImpl",1);M(list,"get_Count");M(list,"get_Item",1);
  M(pn,"get_CurrentRoom");M(pn,"get_IsMessageQueueRunning");M(pn,"get_IsConnectedAndReady");M(view,"get_IsMine");M(view,"get_ViewID");
  M(animator,"get_parameters");M(animator,"SetBoolString",2);M(animator,"GetBoolString",1);M(parameter,"get_name");M(parameter,"get_type");M(emote,"Play",1);M(emote,"hzb");M(pun,"get_photonView");
  F(l,"localPlayerMesh");F(lm,"currentPlayerLobby");for(auto name:{"emotes","animator","pv"})F(emote,name);
  F(binding,"Id");F(binding,"Parameter");F(view,"ObservedComponents");F(view,"Synchronization",true);
  F(stream,"m_Animator");F(stream,"m_SynchronizeParameters");F(sync,"Name");F(sync,"Type",true);F(sync,"SynchronizeType",true);
  room=O(obj);launcher=O(l);lobby=O(lm);garden=O(go);menu=O(go);Transform(garden);Transform(menu);
  launcher->refs["localPlayerMesh"]=garden;lobby->refs["currentPlayerLobby"]=menu;
  garden_anim=O(animator);menu_anim=O(animator);Transform(garden_anim,garden);Transform(menu_anim,menu);
  garden_anim->refs["parameters"]=Params();menu_anim->refs["parameters"]=Params();
  garden->children[animator]={garden_anim};menu->children[animator]={menu_anim};
  pv=O(view);pv->number=1001;Transform(pv,garden);garden->children[view]={pv};pv->ints["Synchronization"]=1;
  preview=O(emote);Transform(preview,menu);source_bindings=O(list);
  int i=0;for(auto id:{"dance-a","dance-b"}){auto* b=O(binding);b->refs["Id"]=S(id);b->refs["Parameter"]=S(i++?"danceParamB":"danceParamA");source_bindings->items.push_back(b);}
  preview->refs["emotes"]=source_bindings;preview->refs["animator"]=menu_anim;preview->refs["pv"]=nullptr;menu->children[emote]={preview};
 }
 void UseModern(bool with_view=false){
  auto* rooms=C("","PhotonRoomsManager",launcher->klass->parent);script_class=C("","PlayerLobbyScript",preview->klass->parent);
  auto* player=C("Photon.Realtime","Player");auto* decoder=C("<PrivateImplementationDetails>{B6FBF95C-D343-4B5E-9090-977A04841BC0}","a");
  Class* pn=nullptr;Class* component=nullptr;for(auto& c:classes){if(c->name=="PhotonNetwork")pn=c.get();if(c->name=="Component")component=c.get();}
  M(pn,"get_LocalPlayer");M(player,"get_ActorNumber");M(component,"get_gameObject");M(go,"get_tag");M(decoder,"of");
  F(rooms,"gameStarted",true);F(script_class,"ownerActorId",true);
  modern=O(rooms);local_actor=O(player);local_actor->number=7;
  actor_script=O(script_class);Transform(actor_script,garden);actor_script->refs["gameObject"]=garden;actor_script->ints["ownerActorId"]=7;
  garden->children[script_class]={actor_script};garden->refs["tag"]=S("room-tag-from-game");scripts={actor_script};
  auto* preview_script=O(script_class);Transform(preview_script,menu);preview_script->refs["gameObject"]=menu;preview_script->ints["ownerActorId"]=7;scripts.push_back(preview_script);
  menu->refs["tag"]=S("room-tag-from-game");menu->children[script_class]={preview_script};
  launcher->refs["localPlayerMesh"]=nullptr;if(!with_view){garden->children[view].clear();if(native)native->refs["pv"]=nullptr;}
 }
 void UseNative(){native=O(emote);Transform(native,garden);native->refs["emotes"]=source_bindings;native->refs["animator"]=garden_anim;native->refs["pv"]=pv;garden->children[emote]={native};}
 void UseStream(){
  stream_component=O(stream);Transform(stream_component,garden);stream_component->refs["m_Animator"]=garden_anim;stream_component->refs["photonView"]=pv;
  auto* params=O(list);for(auto name:{"danceParamA","danceParamB"}){auto* p=O(sync);p->refs["Name"]=S(name);p->ints["Type"]=4;p->ints["SynchronizeType"]=1;params->items.push_back(p);}
  stream_component->refs["m_SynchronizeParameters"]=params;garden->children[stream]={stream_component};auto* observers=O(list);observers->items={stream_component};pv->refs["ObservedComponents"]=observers;
 }
};
Domain* DomainGet(){return &f->domain;}const Assembly** Assemblies(Domain*,size_t* n){*n=1;return f->assemblies;}
const Image* ImageGet(const Assembly*){return &f->image;}
Class* ClassGet(const Image*,const char* ns,const char* name){++f->lookups;for(auto& c:f->classes)if(c->ns==ns&&c->name==name)return c.get();return nullptr;}
Class* Parent(Class* c){return c->parent;}
const Method* MethodGet(Class* c,const char* name,int n){const auto it=c->methods.find({name,n});return it==c->methods.end()?nullptr:&it->second;}
Field* FieldGet(Class* c,const char* name){auto it=c->fields.find(name);return it==c->fields.end()?nullptr:&it->second;}
Class* ObjectClass(Object* o){assert(o);return o->klass;}
void FieldRead(Object* o,Field* field,void* out){assert(o&&field);
 if(field->name=="gameStarted")*static_cast<bool*>(out)=o->ints[field->name]!=0;else if(field->scalar)*static_cast<int*>(out)=o->ints[field->name];else *static_cast<Object**>(out)=o->refs[field->name];}
const Type* ClassType(Class* c){return &(f->types[c]={c});}Object* TypeObject(const Type* t){auto* o=f->O(f->reflection);o->reflection=t->klass;return o;}
Handle Root(Object* o,bool pin){assert(o&&!pin);const auto h=++f->next;f->roots[h]=o;return h;}Object* Target(Handle h){return f->roots.at(h);}void Free(Handle h){assert(f->roots.erase(h)==1);}
void* Unbox(Object* o){return o->klass==f->boolean?static_cast<void*>(&o->flag):static_cast<void*>(&o->number);}
int32_t StringLength(String* s){return reinterpret_cast<Object*>(s)->text.size();}const uint16_t* StringChars(String* s){return reinterpret_cast<const uint16_t*>(reinterpret_cast<Object*>(s)->text.data());}
String* StringNew(const char* s){return f->fail_allocate?nullptr:reinterpret_cast<String*>(f->S(s));}
std::string Text(Object* s){return std::string(s->text.begin(),s->text.end());}
bool Below(Object* a,Object* b){for(;a;a=a->parent)if(a==b)return true;return false;}
Object* Invoke(const Method* method,void* instance,void** args,Object** error){
 ++f->calls;*error=nullptr;auto* self=static_cast<Object*>(instance);const auto& name=method->name;
 if(name=="get_LocalPlayer")return f->local_actor;
 if(name=="get_ActorNumber")return f->I(self->number);
 if(name=="get_gameObject")return self->refs["gameObject"];
 if(name=="get_tag")return self->refs["tag"];
 if(name=="of")return f->S("room-tag-from-game");
 if(name=="get_CurrentRoom")return f->room;
 if(name=="get_IsMessageQueueRunning")return f->B(f->queue);
 if(name=="get_IsConnectedAndReady")return f->B(f->connected);
 if(name=="op_Implicit")return f->B(args[0]&&static_cast<Object*>(args[0])->alive);
 if(name=="GetInstanceID")return f->I(self->instance);
 if(name=="get_isActiveAndEnabled"||name=="get_activeInHierarchy")return f->B(self->active&&self->alive);
 if(name=="get_transform")return self->refs.at("transform");
 if(name=="IsChildOf")return f->B(Below(self,static_cast<Object*>(args[0])));
 if(name=="get_IsMine"){assert(self==f->pv);return f->B(f->own);}if(name=="get_ViewID")return f->I(self->number);
 if(name=="get_Length"||name=="get_Count")return f->I(self->items.size());
 if(name=="GetValueImpl"||name=="get_Item")return self->items.at(*static_cast<int*>(args[0]));
 if(name=="get_parameters"){if(f->fail_parameters){*error=f->room;return nullptr;}return self->refs["parameters"];}
 if(name=="get_name")return self->refs["name"];
 if(name=="get_type")return f->I(self->number);
 if(name=="get_photonView")return self->refs["photonView"];
 if(name=="FindObjectsByType"){
  auto* type=static_cast<Object*>(args[0]);const int inactive=*(int*)args[1];assert(*(int*)args[2]==0);
  if(type->reflection==f->launcher->klass)assert(inactive==1);
  else if(f->modern&&type->reflection==f->modern->klass)assert(inactive==1);
  else if(type->reflection==f->lobby->klass)assert(inactive==0||inactive==1);else assert(inactive==0);
  auto* a=f->O(f->array);
  if(type->reflection==f->launcher->klass){a->items={f->launcher};
 if(f->duplicate_launcher)a->items.push_back(f->launcher);}
  if(type->reflection==f->lobby->klass)a->items={f->lobby};
  if(f->modern&&type->reflection==f->modern->klass)a->items={f->modern};
  if(f->script_class&&type->reflection==f->script_class)a->items=f->scripts;
  return a;
 }
 if(name=="GetComponentsInternal"){
  assert(self==f->garden||self==f->menu);assert(!*(bool*)args[1]&&*(bool*)args[2]&&!*(bool*)args[4]&&!args[5]);
  assert(*(bool*)args[3]==(self==f->menu&&static_cast<Object*>(args[0])->reflection==f->emote));
  if(f->fail_children){*error=f->room;return nullptr;}auto* a=f->O(f->array);a->items=self->children[static_cast<Object*>(args[0])->reflection];return a;
 }
 if(name=="SetBoolString"){
  assert(self==f->garden_anim&&self!=f->menu_anim);assert(args[0]&&static_cast<Object*>(args[0])->klass==f->string);++f->sets;
  if(f->fail_set){*error=f->room;return nullptr;}self->bools[Text(static_cast<Object*>(args[0]))]=*(bool*)args[1];
  if(f->leave_on_set)f->room=nullptr;
 if(f->change_owner_on_set)f->actor_script->ints["ownerActorId"]=99;
 if(f->reenter)f->reenter->Tick(100);
  return nullptr;
 }
 if(name=="GetBoolString"){assert(self==f->garden_anim);auto* result=f->B(self->bools[Text(static_cast<Object*>(args[0]))]);if(f->leave_on_readback)f->room=nullptr;return result;}
 if(name=="Play"||name=="hzb"){
  assert(self==f->native&&self!=f->preview);
 if(f->fail_play){*error=f->room;return nullptr;}
  if(name=="Play"){++f->plays;assert(args[0]);}else{++f->stops;assert(!args);}
  if(!f->ignore_play){f->garden_anim->bools["danceParamA"]=name=="Play"&&Text((Object*)args[0])=="dance-a";
   f->garden_anim->bools["danceParamB"]=name=="Play"&&Text((Object*)args[0])=="dance-b";}
  if(f->leave_on_play)f->room=nullptr;
 if(f->reenter)f->reenter->Tick(100);
  return nullptr;
 }
 assert(false&&"Unexpected mutation or call");return nullptr;
}
Api API(){Api a{};a.domain_get=DomainGet;a.domain_get_assemblies=Assemblies;a.assembly_get_image=ImageGet;a.class_from_name=ClassGet;a.class_get_parent=Parent;
 a.class_get_method_from_name=MethodGet;a.object_get_class=ObjectClass;a.class_get_field_from_name=FieldGet;a.field_get_value=FieldRead;a.class_get_type=ClassType;a.type_get_object=TypeObject;
 a.gchandle_new=Root;a.gchandle_get_target=Target;a.gchandle_free=Free;a.object_unbox=Unbox;a.string_length=StringLength;a.string_chars=StringChars;a.string_new=StringNew;a.runtime_invoke=Invoke;return a;}
void Begin(lobby_emotes::Controller& c){c.Tick(0);c.Tick(2.9);assert(!f->calls&&!f->lookups);c.Tick(3);assert(f->roots.empty());}
void Select(lobby_emotes::Controller& c,double now,const std::string& id){auto snapshot=lobby_emote_bridge::Read();assert(snapshot.ready);lobby_emote_bridge::Request(snapshot.generation,id);c.Tick(now);assert(f->roots.empty());}
void MenuUntouched(){assert(f->menu_anim->bools.empty());}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
int main(){using S=lobby_emotes::State;
 for(bool native:{false,true}){Fake f;
 if(native){f.UseNative();}f.UseStream();auto api=API();lobby_emotes::Controller c(api);f.reenter=&c;Begin(c);
  auto snapshot=lobby_emote_bridge::Read();assert(snapshot.ready&&snapshot.ids.size()==2&&c.GetState()==S::Ready);
  Select(c,3.6,"dance-a");assert(f.garden_anim->bools["danceParamA"]&&!f.garden_anim->bools["danceParamB"]&&c.GetState()==S::Played);
  assert(lobby_emote_bridge::Read().status.find(u8"diğer cihazdan")!=std::string::npos);MenuUntouched();
  const int calls=f.plays+f.sets;c.Tick(4.2);assert(calls==f.plays+f.sets);
  Select(c,4.8,"dance-b");assert(!f.garden_anim->bools["danceParamA"]&&f.garden_anim->bools["danceParamB"]);
  Select(c,5.4,"");assert(!f.garden_anim->bools["danceParamB"]&&c.GetState()==S::Stopped);MenuUntouched();
  Select(c,6,"forged-id");assert(c.GetState()==S::Ready&&!f.garden_anim->bools["forged-id"]);
  f.room=nullptr;c.Tick(6.6);assert((lobby_emote_bridge::Read().visible&&!lobby_emote_bridge::Read().ready));MenuUntouched();
 }
 for(int scenario=0;scenario<11;++scenario){Fake f;auto api=API();lobby_emotes::Controller c(api);S expected=S::NoRoom;
  switch(scenario){case 0:f.room=nullptr;break;case 1:f.queue=false;expected=S::Loading;break;
   case 2:f.launcher->refs["localPlayerMesh"]=nullptr;expected=S::NoGarden;break;
   case 3:f.own=false;expected=S::ForeignPlayer;break;case 4:f.pv->number=0;expected=S::ForeignPlayer;break;
   case 5:f.garden_anim->active=false;expected=S::NoAnimator;break;
   case 6:f.source_bindings->items.clear();expected=S::NoEmotes;break;
   case 7:f.duplicate_launcher=true;expected=S::NoGarden;break;
   case 8:f.fail_parameters=true;expected=S::NoAnimator;break;
   case 9:api.field_get_value=nullptr;expected=S::MetadataMissing;break;
   case 10:f.garden_anim->refs["transform"]->parent=f.menu->refs["transform"];expected=S::NoAnimator;break;}
  Begin(c);assert(c.GetState()==expected&&!f.plays&&!f.sets&&f.roots.empty());
  auto ui=lobby_emote_bridge::Read();assert(ui.visible&&!ui.ready&&ui.status.find("v19 | ")==0);
  lobby_emote_bridge::Request(ui.generation,"dance-a");assert(!lobby_emote_bridge::Take().pending);MenuUntouched();
 }
 {Fake f;auto* other=f.O(f.animator);f.Transform(other,f.garden);other->refs["parameters"]=f.Params();f.garden->children[f.animator].push_back(other);
  auto api=API();lobby_emotes::Controller c(api);Begin(c);assert(c.GetState()==S::AmbiguousAnimator&&!lobby_emote_bridge::Read().ready);}
 {Fake f;f.UseNative();f.native->refs["animator"]=f.menu_anim;auto api=API();lobby_emotes::Controller c(api);Begin(c);Select(c,3.6,"dance-a");assert(!f.plays&&f.sets==3);MenuUntouched();}
 {Fake f;f.UseNative();f.native->refs["emotes"]=f.O(f.list);auto api=API();lobby_emotes::Controller c(api);Begin(c);Select(c,3.6,"dance-a");assert(!f.plays&&f.sets==3);MenuUntouched();}
 {Fake f;for(auto* p:f.garden_anim->refs["parameters"]->items)p->number=1;auto api=API();lobby_emotes::Controller c(api);Begin(c);assert(c.GetState()==S::NoAnimator&&!lobby_emote_bridge::Read().ready);}
 {Fake f;auto api=API();lobby_emotes::Controller c(api);Begin(c);const auto gen=lobby_emote_bridge::Read().generation;
  lobby_emote_bridge::Request(gen,"dance-a");f.garden_anim->instance=++f.ids;c.Tick(3.6);assert(!f.sets&&lobby_emote_bridge::Read().generation!=gen);
  Select(c,4.2,"dance-a");assert(f.sets==3);lobby_emote_bridge::Request(lobby_emote_bridge::Read().generation,"dance-b");f.room=f.O(f.room->klass);c.Tick(4.8);assert(f.sets==3);MenuUntouched();}
 {Fake f;auto api=API();lobby_emotes::Controller c(api);Begin(c);auto gen=lobby_emote_bridge::Read().generation;
  lobby_emote_bridge::Request(gen,"dance-a");f.queue=false;c.Tick(3.6);f.queue=true;c.Tick(4.2);assert(!f.sets&&lobby_emote_bridge::Read().generation!=gen);}
 for(bool native:{false,true}){Fake f;
 if(native){f.UseNative();}auto api=API();lobby_emotes::Controller c(api);Begin(c);
  if(native)f.leave_on_play=true;else f.leave_on_set=true;Select(c,3.6,"dance-a");assert(c.GetState()==S::SceneChanged&&(lobby_emote_bridge::Read().visible&&!lobby_emote_bridge::Read().ready));MenuUntouched();}
 {Fake f;auto api=API();lobby_emotes::Controller c(api);Begin(c);f.leave_on_readback=true;Select(c,3.6,"dance-a");assert(c.GetState()==S::SceneChanged&&(lobby_emote_bridge::Read().visible&&!lobby_emote_bridge::Read().ready));MenuUntouched();}
 for(int scenario=0;scenario<4;++scenario){Fake f;
 if(scenario<2){f.UseNative();}auto api=API();lobby_emotes::Controller c(api);Begin(c);
  switch(scenario){case 0:f.fail_play=true;break;case 1:f.ignore_play=true;break;case 2:f.fail_set=true;break;case 3:f.fail_allocate=true;break;}
  Select(c,3.6,"dance-a");assert(c.GetState()==S::Failed);MenuUntouched();}
 for(int scenario=0;scenario<4;++scenario){Fake f;f.UseStream();
  switch(scenario){case 0:f.pv->refs["ObservedComponents"]->items.clear();break;
   case 1:f.stream_component->refs["m_Animator"]=f.menu_anim;break;case 2:f.pv->ints["Synchronization"]=0;break;
   case 3:for(auto* p:f.stream_component->refs["m_SynchronizeParameters"]->items)p->ints["SynchronizeType"]=0;break;}
  auto api=API();lobby_emotes::Controller c(api);Begin(c);Select(c,3.6,"dance-a");assert(c.GetState()==S::Played&&lobby_emote_bridge::Read().status.find(u8"ağ kanalı doğrulanamadı")!=std::string::npos);MenuUntouched();}
 {Fake f;f.launcher->active=false;auto api=API();lobby_emotes::Controller c(api);Begin(c);
  assert(c.GetState()==S::Ready&&!f.launcher->active);Select(c,3.6,"dance-a");assert(f.sets==3&&!f.launcher->active);MenuUntouched();}
 {Fake f;f.launcher->refs["localPlayerMesh"]=nullptr;auto api=API();lobby_emotes::Controller c(api);Begin(c);
  auto ui=lobby_emote_bridge::Read();assert(ui.visible&&!ui.ready&&ui.ids.size()==2&&c.GetState()==S::NoGarden);
  assert(ui.status.find(u8"yönetici=1, canlı=0, etkin=0")!=std::string::npos);
  f.launcher->refs["localPlayerMesh"]=f.garden;c.Tick(3.6);assert(lobby_emote_bridge::Read().ready);Select(c,4.2,"dance-a");assert(f.sets==3);MenuUntouched();}
 {Fake f;f.animator->methods.erase({"SetBoolString",2});auto api=API();lobby_emotes::Controller c(api);Begin(c);
  auto ui=lobby_emote_bridge::Read();assert(ui.visible&&!ui.ready&&ui.status.find("Animator.SetBoolString")!=std::string::npos);c.Tick(8);assert(lobby_emote_bridge::Read().visible);MenuUntouched();}
 {Fake f;auto api=API();lobby_emotes::Controller c(api);Begin(c);auto ui=lobby_emote_bridge::Read();
  lobby_emote_bridge::Request(ui.generation,"dance-a");assert(lobby_emote_bridge::command.pending);
  ui.ready=false;lobby_emote_bridge::Publish(ui);assert(!lobby_emote_bridge::Take().pending);MenuUntouched();}
 for(bool native:{false,true}){Fake f;if(native){f.UseNative();}f.UseModern();auto api=API();lobby_emotes::Controller c(api);Begin(c);
  assert(lobby_emote_bridge::Read().ready&&f.launcher->refs["localPlayerMesh"]==nullptr&&f.garden->children[f.view].empty());
  Select(c,3.6,"dance-a");assert(c.GetState()==S::Played&&f.garden_anim->bools["danceParamA"]);MenuUntouched();
  Select(c,4.2,"");assert(c.GetState()==S::Stopped&&!f.garden_anim->bools["danceParamA"]);MenuUntouched();}
 for(int scenario=0;scenario<5;++scenario){Fake f;f.UseModern();
  switch(scenario){case 0:f.actor_script->ints["ownerActorId"]=99;break;case 1:f.garden->refs["tag"]=f.S("wrong-tag");break;
   case 2:f.modern->ints["gameStarted"]=1;break;case 3:f.actor_script->active=false;break;
   case 4:f.scripts.push_back(f.actor_script);break;}
  auto api=API();lobby_emotes::Controller c(api);Begin(c);assert(!lobby_emote_bridge::Read().ready&&!f.sets&&!f.plays);MenuUntouched();}
 {Fake f;f.UseModern(true);f.own=false;auto api=API();lobby_emotes::Controller c(api);Begin(c);assert(!lobby_emote_bridge::Read().ready&&!f.sets);MenuUntouched();}
 {Fake f;f.UseModern(true);f.pv->number=0;f.pv->active=false;f.own=false;auto api=API();lobby_emotes::Controller c(api);Begin(c);assert(lobby_emote_bridge::Read().ready);Select(c,3.6,"dance-a");assert(f.sets==3);MenuUntouched();}
 {Fake f;f.UseModern();f.lobby->active=f.menu->active=f.preview->active=false;auto api=API();lobby_emotes::Controller c(api);Begin(c);assert(lobby_emote_bridge::Read().ready);Select(c,3.6,"dance-a");assert(f.sets==3&&!f.menu->active&&!f.preview->active);MenuUntouched();}
 {Fake f;f.UseModern();auto api=API();lobby_emotes::Controller c(api);Begin(c);auto before=lobby_emote_bridge::Read();
  lobby_emote_bridge::Request(before.generation,"dance-a");f.local_actor->number=8;f.actor_script->ints["ownerActorId"]=8;c.Tick(3.6);
  assert(!f.sets&&lobby_emote_bridge::Read().generation!=before.generation);Select(c,4.2,"dance-a");assert(f.sets==3);MenuUntouched();}
 {Fake f;f.UseModern();auto api=API();lobby_emotes::Controller c(api);Begin(c);auto before=lobby_emote_bridge::Read();
  lobby_emote_bridge::Request(before.generation,"dance-a");f.modern->instance=++f.ids;c.Tick(3.6);assert(!f.sets&&lobby_emote_bridge::Read().generation!=before.generation);}
 {Fake f;f.UseModern();auto api=API();lobby_emotes::Controller c(api);Begin(c);f.change_owner_on_set=true;Select(c,3.6,"dance-a");assert(c.GetState()==S::SceneChanged&&!f.garden_anim->bools["danceParamA"]);MenuUntouched();}
 lobby_emote_bridge::Publish({});
 std::cout<<"V19 actor/tag-owned local waiting clones with empty legacy mesh/no PhotonView, explicit preview exclusion and actor/manager generation guards; visible waits/errors, disabled launcher discovery, disabled catalogue/command cleanup; garden retarget: separate menu/network mesh, read-only binding source, actual Bool support/hierarchy/ownership, native/fallback playback, no preview mutation, stale room/animator commands, ambiguity, readback/errors/reentry and read-only observed network stream: PASS\n";
}
