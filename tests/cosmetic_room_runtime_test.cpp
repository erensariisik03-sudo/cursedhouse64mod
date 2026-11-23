#include "cosmetic_room.h"
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
namespace il2cpp {
struct Class;
struct Method { std::string owner,name; };
struct Field { std::string name; };
struct Class { std::string ns,name;std::map<std::pair<std::string,int>,Method> methods;std::map<std::string,Field> fields; };
struct Type { Class* klass; };
struct Object { Class* klass;std::u16string text;bool flag=true;int number=0;bool active=true;Class* reflection=nullptr;std::vector<Object*> items;std::map<std::u16string,Object*> props; };
struct String: Object{};struct Domain{};struct Assembly{};struct Image{};
}
#include "../src/cosmetic_room.cpp"
using namespace il2cpp;
namespace {
struct Fake;
Fake* f;
struct Fake {
 Domain domain;Assembly assembly;Image image;const Assembly* assemblies[1]={&assembly};
 std::vector<std::unique_ptr<Class>> classes;std::vector<std::unique_ptr<Object>> objects;
 std::map<Class*,Type> types;std::map<Handle,Object*> roots;Handle next=0;
 Object *locker{},*manager{},*local{},*room{},*account{},*table{},*profile{},*equipped{};
 Object* equipped_values[2]{};
 Class *strings{},*booleans{},*integers{},*arrays{},*reflections{},*tables{};
 Object* selected[2]{};Object* keys[2]{};
 Object *room_name=nullptr,*joined_name=nullptr;bool in_room=true,leave_result=true;int leaves=0,joins=0;
 bool dirty[2]{false,false},connected=true,fail_selection=false,fail_publish=false,update_props=true,fail_uid=false;
 int lookups=0,calls=0,published[2]{0,0},equipped_writes=0;
 Class* C(const char* ns,const char* name){auto v=std::make_unique<Class>();v->ns=ns;v->name=name;auto* p=v.get();classes.push_back(std::move(v));return p;}
 Object* O(Class* c){auto o=std::make_unique<Object>();o->klass=c;auto* p=o.get();objects.push_back(std::move(o));return p;}
 Object* S(const char* s){auto* p=O(strings);for(;*s;++s)p->text+=static_cast<unsigned char>(*s);return p;}
 void M(Class* c,const char* name,int count){c->methods[{name,count}]={c->name,name};}
 Object* B(bool b){auto* o=O(booleans);o->flag=b;return o;}
 Object* I(int i){auto* o=O(integers);o->number=i;return o;}
 Fake(){
  f=this;strings=C("System","String");booleans=C("System","Boolean");integers=C("System","Int32");arrays=C("System","Array");reflections=C("System","Type");tables=C("","Hashtable");
  auto* l=C("","LockerManager");auto* m=C("","PhotonManager");auto* n=C("Photon.Pun","PhotonNetwork");auto* p=C("Photon.Realtime","Player");
  auto* u=C("","fi");auto* r=C("UnityEngine","Resources");auto* b=C("UnityEngine","Behaviour");auto* o=C("UnityEngine","Object");
  auto* et=C("","et");auto* ew=C("","ew");M(u,"cvd",0);M(et,"cqj",0);
  M(ew,"crj",0);M(ew,"crh",0);M(ew,"crk",1);M(ew,"cri",1);
  auto* d=C("<PrivateImplementationDetails>{B6FBF95C-D343-4B5E-9090-977A04841BC0}","a");
  M(l,"flh",0);M(l,"flj",0);l->fields["pyc"]={"pyc"};l->fields["pyd"]={"pyd"};
  M(m,"gna",1);M(m,"gnb",1);M(n,"get_IsConnectedAndReady",0);M(n,"get_LocalPlayer",0);M(n,"get_CurrentRoom",0);
  auto* rc=C("Photon.Realtime","Room");M(rc,"get_Name",0);M(rc,"get_PlayerCount",0);M(rc,"get_IsOpen",0);
  M(n,"get_InRoom",0);M(n,"LeaveRoom",1);M(n,"JoinRoom",2);
  M(p,"get_CustomProperties",0);M(u,"cvb",0);M(r,"FindObjectsOfTypeAll",1);M(o,"FindObjectsByType",3);M(b,"get_isActiveAndEnabled",0);M(o,"op_Implicit",1);
  M(arrays,"get_Length",0);M(arrays,"GetValueImpl",1);M(d,"dms",0);M(d,"dmt",0);M(tables,"ContainsKey",1);M(tables,"get_Item",1);
  locker=O(l);manager=O(m);local=O(p);room=nullptr;account=S("account-a");table=O(tables);
  profile=O(et);equipped=O(ew);equipped_values[0]=S("old-hat");equipped_values[1]=S("old-skin");
  selected[0]=S("hat-a");selected[1]=S("skin-a");keys[0]=S("decoded-accessory-key");keys[1]=S("decoded-skin-key");
 }
};
Domain* DomainGet(){return &f->domain;}
const Assembly** Assemblies(Domain*,size_t* count){*count=1;return f->assemblies;}
const Image* ImageGet(const Assembly*){return &f->image;}
Class* ClassGet(const Image*,const char* ns,const char* name){++f->lookups;for(auto& c:f->classes)if(c->ns==ns&&c->name==name)return c.get();return nullptr;}
Class* Parent(Class*){return nullptr;}
const Method* MethodGet(Class* c,const char* name,int count){auto i=c->methods.find({name,count});return i==c->methods.end()?nullptr:&i->second;}
Class* ObjectClass(Object* o){return o->klass;}
Field* FieldGet(Class* c,const char* name){auto i=c->fields.find(name);return i==c->fields.end()?nullptr:&i->second;}
void Read(Object* o,Field* field,void* out){assert(o==f->locker);*static_cast<bool*>(out)=f->dirty[field->name=="pyc"?0:1];}
const Type* ClassType(Class* c){return &(f->types[c]={c});}
Object* TypeObject(const Type* t){auto* o=f->O(f->reflections);o->reflection=t->klass;return o;}
Handle Root(Object* o,bool pinned){assert(o&&!pinned);auto h=++f->next;f->roots[h]=o;return h;}
Object* Target(Handle h){auto i=f->roots.find(h);return i==f->roots.end()?nullptr:i->second;}
void Free(Handle h){assert(f->roots.erase(h)==1);}
String* StringNew(const char* s){return reinterpret_cast<String*>(f->S(s));}
int32_t Length(String* s){return reinterpret_cast<Object*>(s)->text.size();}
const uint16_t* Chars(String* s){return reinterpret_cast<const uint16_t*>(reinterpret_cast<Object*>(s)->text.data());}
void* Unbox(Object* o){return o->klass==f->booleans?static_cast<void*>(&o->flag):static_cast<void*>(&o->number);}
Object* Invoke(const Method* m,void* instance,void** args,Object** error){
 ++f->calls;*error=nullptr;auto* self=static_cast<Object*>(instance);const auto& n=m->name;
 if(n=="cvb"){if(f->fail_uid){*error=f->manager;return nullptr;}return f->account;}
 if(n=="cvd")return f->profile;
 if(n=="cqj"){assert(self==f->profile);return f->equipped;}
 if(n=="crj"||n=="crh"){assert(self==f->equipped);return f->equipped_values[n=="crj"?0:1];}
 if(n=="crk"||n=="cri"){
  auto* value=static_cast<Object*>(args[0]);assert(self==f->equipped&&value->klass==f->strings);
  ++f->equipped_writes;
  f->equipped_values[n=="crk"?0:1]=value;return nullptr;
 }
 if(n=="get_IsConnectedAndReady")return f->B(f->connected);
 if(n=="get_LocalPlayer")return f->local;
 if(n=="get_CurrentRoom")return f->room;
 if(n=="get_InRoom")return f->B(f->in_room);
 if(n=="get_Name")return f->room_name;
 if(n=="get_PlayerCount")return f->I(2);
 if(n=="get_IsOpen")return f->B(true);
 if(n=="LeaveRoom"){++f->leaves;assert(!*static_cast<bool*>(args[0]));return f->B(f->leave_result);}
 if(n=="JoinRoom"){++f->joins;f->joined_name=static_cast<Object*>(args[0]);assert(args[1]==nullptr);return f->B(true);}
 if(n=="get_CustomProperties"){assert(self==f->local);return f->table;}
 if(n=="op_Implicit")return f->B(args[0]!=nullptr);
 if(n=="get_isActiveAndEnabled")return f->B(self->active);
 if(n=="get_Length")return f->I(self->items.size());
 if(n=="GetValueImpl"){int i=*static_cast<int*>(args[0]);assert(i>=0&&i<static_cast<int>(self->items.size()));return self->items[i];}
 if(n=="FindObjectsByType"){
  assert(*static_cast<int*>(args[1])==0&&*static_cast<int*>(args[2])==0);
  auto* type=static_cast<Object*>(args[0]);assert(type->klass==f->reflections);auto* a=f->O(f->arrays);
  if(type->reflection==f->locker->klass)a->items={f->locker};
  if(type->reflection==f->manager->klass)a->items={f->manager};
  return a;
 }
 if(n=="flh"||n=="flj"){if(f->fail_selection)*error=f->manager;return f->selected[n=="flh"?0:1];}
 if(n=="dms"||n=="dmt")return f->keys[n=="dms"?0:1];
 if(n=="ContainsKey"||n=="get_Item"){
  auto* key=static_cast<Object*>(args[0]);assert(key->klass==f->strings&&self==f->table);
  if(n=="ContainsKey")return f->B(self->props.count(key->text)!=0);
  assert(self->props.count(key->text));return self->props[key->text];
 }
 if(n=="gna"||n=="gnb"){
  int slot=n=="gna"?0:1;auto* value=static_cast<Object*>(args[0]);assert(self==f->manager&&value->klass==f->strings);
  ++f->published[slot];if(f->fail_publish){*error=f->manager;return nullptr;}
  if(f->update_props)f->table->props[f->keys[slot]->text]=value;
  return nullptr;
 }
 assert(false&&"Unexpected managed call (no cloud/purchase/foreign-player operation allowed)");return nullptr;
}
Api API(){Api a{};a.domain_get=DomainGet;a.domain_get_assemblies=Assemblies;a.assembly_get_image=ImageGet;a.class_from_name=ClassGet;a.class_get_method_from_name=MethodGet;a.class_get_parent=Parent;a.object_get_class=ObjectClass;a.class_get_field_from_name=FieldGet;a.field_get_value=Read;a.class_get_type=ClassType;a.type_get_object=TypeObject;a.gchandle_new=Root;a.gchandle_get_target=Target;a.gchandle_free=Free;a.string_new=StringNew;a.string_length=Length;a.string_chars=Chars;a.object_unbox=Unbox;a.runtime_invoke=Invoke;return a;}
void Run(cosmetic_room::Runtime& r,double start,int n=40){for(int i=0;i<n;++i)r.Tick(start+i*.1);}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
int main(){
 // Keep an opaque room name as the exact managed string. It can exceed the
 // wardrobe limit, contain non-BMP emoji/rich text, or even embedded NULs.
 for(int scenario=0;scenario<3;++scenario){Fake f;auto api=API();{
  cosmetic_room::Runtime r(api);assert(r.Resolve());f.room=f.O(f.arrays);f.room_name=f.S("unused");
  f.room_name->text=scenario==0?std::u16string(600,u'名'):u"<size=999><color=#AABBCC>\U0001F600 Oda İı🕷️</color></size>";
  if(scenario==2)f.room_name->text.insert(4,1,u'\0');
  auto* exact=f.room_name;const auto contents=exact->text;
  assert(!r.Text(exact)||scenario==1); // old wardrobe conversion isn't the room-ID path
  r.StartRejoin(100,f.room);assert(r.rejoin==1&&f.leaves==1&&r.rejoin_name.Get()==exact);
  f.room_name=nullptr;f.in_room=false;r.StepRejoin(101);assert(r.rejoin==2&&f.joins==1&&f.joined_name==exact&&f.joined_name->text==contents);
  f.in_room=true;r.StepRejoin(102);assert(!r.rejoin&&!r.rejoin_name.Get());
 }assert(f.roots.empty());}
 {Fake f;auto api=API();{cosmetic_room::Runtime r(api);assert(r.Resolve());f.room=f.O(f.arrays);
  f.room_name=f.S("");r.StartRejoin(100,f.room);assert(!f.leaves&&!r.rejoin);
  f.room_name=f.S("valid");f.leave_result=false;r.StartRejoin(101,f.room);assert(f.leaves==1&&!r.rejoin&&!r.rejoin_name.Get());
  f.leave_result=true;r.StartRejoin(102,f.room);assert(r.rejoin_name.Get());r.StepRejoin(128);assert(!r.rejoin&&!r.rejoin_name.Get());
 }assert(f.roots.empty());}

 {Fake f;auto api=API();{cosmetic_room::Runtime r(api);r.Tick(0);r.Tick(2.9);assert(!f.calls&&!f.lookups);
  Run(r,3);assert(!f.published[0]&&!f.published[1]);
  f.dirty[0]=f.dirty[1]=true;Run(r,8);assert(f.published[0]==1&&f.published[1]==1);
  assert(f.table->props[f.keys[1]->text]->text==u"skin-a");
  assert(f.equipped_values[1]->text==u"skin-a"&&f.equipped_values[0]->text==u"hat-a");
  f.locker->active=false;f.selected[1]=f.S("old-skin-from-cloud");Run(r,13);assert(f.published[1]==1&&r.plan.Wanted(1)=="skin-a"&&!r.locker.Get());
  for(const auto& root:f.roots)assert(root.second!=f.locker);
  r.Suspend();assert(!r.manager.Get()&&!r.locker.Get()&&r.plan.Wanted(1)=="skin-a");
  for(const auto& root:f.roots)assert(root.second!=f.manager&&root.second!=f.locker);
  f.room=f.O(f.arrays);Run(r,18);assert(f.published[1]==2&&f.table->props[f.keys[1]->text]->text==u"skin-a");
  f.connected=false;Run(r,23);assert(f.published[1]==2);f.connected=true;Run(r,28);assert(f.published[1]==3);
  f.account=f.S("account-b");f.locker->active=true;Run(r,33);assert(!r.plan.Wanted(1)&&f.published[1]==3);
  f.dirty[0]=f.dirty[1]=false;Run(r,38);f.dirty[1]=true;f.selected[1]=f.S("new-account-choice");Run(r,43);assert(f.published[1]==4);
 }assert(f.roots.empty());}
 {Fake f;f.dirty[1]=true;f.update_props=false;auto api=API();{cosmetic_room::Runtime r(api);Run(r,0,100);assert(f.published[1]==3);}assert(f.roots.empty());}
 {Fake f;f.dirty[0]=true;f.fail_selection=true;auto api=API();{cosmetic_room::Runtime r(api);Run(r,0,100);assert(!f.published[0]);}assert(f.roots.empty());}
 {Fake f;f.dirty[1]=true;f.fail_publish=true;auto api=API();{cosmetic_room::Runtime r(api);Run(r,0,100);assert(f.published[1]==3);}assert(f.roots.empty());}
 // Leave -> offline profile reload -> temporarily missing UID -> same account.
 // This reproduces the lost outfit independently of Photon readiness.
 for(bool uid_error:{false,true}){
  Fake f;f.dirty[0]=f.dirty[1]=true;auto api=API();{
   cosmetic_room::Runtime r(api);Run(r,0,100);
   assert(r.plan.Wanted(1)=="skin-a"&&r.plan.Wanted(0)=="hat-a");
   const int published=f.published[1];f.connected=false;f.locker->active=false;f.manager->active=false;
   f.equipped=f.O(f.equipped->klass);f.equipped_values[0]=f.S("cloud-hat");f.equipped_values[1]=f.S("cloud-skin");
   Run(r,11,10);
   assert(f.equipped_values[0]->text==u"hat-a"&&f.equipped_values[1]->text==u"skin-a");
   assert(f.published[1]==published); // No offline publication.
   auto* same_account=f.account;f.account=nullptr;f.fail_uid=uid_error;
   const int before=f.equipped_writes;Run(r,13,20);
   assert(r.plan.Wanted(1)=="skin-a"&&r.plan.Wanted(0)=="hat-a"&&f.equipped_writes==before);
   f.account=same_account;f.fail_uid=false;f.connected=true;f.manager->active=true;Run(r,16,30);
   assert(r.plan.Wanted(1)=="skin-a"&&f.published[1]==published+1);
   // An actual different nonempty account still clears the old choices.
   f.account=nullptr;Run(r,20,10);f.account=f.S("different-account");
   f.equipped_values[1]=f.S("different-account-skin");Run(r,22,20);
   assert(!r.plan.Wanted(0)&&!r.plan.Wanted(1)&&f.equipped_values[1]->text==u"different-account-skin");
  }assert(f.roots.empty());
 }
 std::cout<<"Cosmetic room runtime: Unity delay, original publisher ABI/decoded keys, local readback, lobby/room/reconnect, inactive-preview reset, account-change dirty latch, bounded failures and GC roots: PASS\n";
 std::cout<<"Outfit retention: disconnected profile replacement, inactive manager, transient null/throwing UID, same-account recovery and real-account isolation: PASS\n";
}
