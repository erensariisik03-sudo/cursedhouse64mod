#include "cosmetic_catalog.h"
#include "wardrobe_bridge.h"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace il2cpp {
struct Method { enum Kind { Library, Profile, GetField, SetField, Constructor, Count, Item, Contains, Add, Remove, Resources, Length, ArrayItem, Notify, View } kind; std::string field; };
struct Field { std::string name; const Type* type=nullptr; };
struct Class {
    std::string ns,name;
    std::map<std::pair<std::string,int>,Method> methods;
    std::map<std::string,Field> fields;
};
struct Object {
    Class* klass;
    std::map<std::string,Object*> fields;
    std::vector<Object*> items;
    std::u16string text;
    int integer=0; bool boolean=false;
};
struct String: Object {};
struct Type { Class* klass; };
struct Domain {}; struct Assembly {}; struct Image {};
}
using namespace il2cpp;
using cosmetic_catalog::Controller;
using cosmetic_catalog::Phase;
namespace {
struct Fixture;
Fixture* g=nullptr;
struct Fixture {
    Domain domain; Assembly assembly; Image image;
    const Assembly* assemblies[1]={&assembly};
    std::vector<std::unique_ptr<Class>> classes;
    std::vector<std::unique_ptr<Object>> objects;
    std::map<Handle,Object*> roots;
    Handle last=0;
    Object *library=nullptr, *profile=nullptr, *array=nullptr, *type_object=nullptr;
    Object* listener=nullptr;
    std::map<std::string,Object*> view_lists;
    Controller* reenter=nullptr;
    Class *string_class, *list_class, *entry_class, *library_class, *profile_class, *bool_class, *int_class;
    Type library_type, list_type;
    int calls=0, adds=0, removes=0, fail_add=0, mutations_per_tick=0;
    int notifications=0;
    bool notification_error=false;
    bool throw_after_add=false, null_profile=false, static_catalog=true, throw_resource=false, throw_getter=false;
    Class* C(const char* ns,const char* name) {
        auto c=std::make_unique<Class>(); c->ns=ns;c->name=name;
        auto* p=c.get();classes.push_back(std::move(c));return p;
    }
    Object* O(Class* c) {
        auto o=std::make_unique<Object>();o->klass=c;
        auto* p=o.get();objects.push_back(std::move(o));return p;
    }
    Object* S(const std::string& text) {
        auto* s=O(string_class);for(unsigned char ch:text)s->text.push_back(ch);return s;
    }
    void M(Class* c,const char* name,int args,Method::Kind kind,const char* field="") {
        c->methods[{name,args}]={kind,field};
    }
    Object* NewProfile() {
        auto* p=O(profile_class);
        for(const char* key:{"skins","accessories","emotes"})p->fields[key]=O(list_class);
        p->fields["skins"]->items.push_back(S("owned_skin"));return p;
    }
    Fixture() {
        g=this;
        string_class=C("System","String");list_class=C("System.Collections.Generic","List`1");
        entry_class=C("","Entry");library_class=C("","CosmeticLibrary");profile_class=C("","ey");
        bool_class=C("System","Boolean");int_class=C("System","Int32");
        auto* shop=C("","kt");auto* cache=C("","fi");auto* resources=C("UnityEngine","Resources");
        auto* array_class=C("System","Array");
        auto* action=C("System","Action");M(action,"Invoke",0,Method::Notify);
        listener=O(action);cache->fields["oly"]={"oly"};
        M(shop,"eql",0,Method::Library);M(cache,"cvf",0,Method::Profile);
        M(cache,"cvz",0,Method::View,"skins");M(cache,"cwa",0,Method::View,"accessories");M(cache,"cwb",0,Method::View,"emotes");
        M(profile_class,"crr",0,Method::GetField,"skins");M(profile_class,"crt",0,Method::GetField,"accessories");
        M(profile_class,"crv",0,Method::GetField,"emotes");
        M(profile_class,"crs",1,Method::SetField,"skins");M(profile_class,"cru",1,Method::SetField,"accessories");
        M(profile_class,"crw",1,Method::SetField,"emotes");M(list_class,".ctor",0,Method::Constructor);
        list_type={list_class};
        for(const char* field:{"<ojw>k__BackingField","<ojx>k__BackingField","<ojy>k__BackingField"})
            profile_class->fields[field]={field,&list_type};
        M(list_class,"get_Count",0,Method::Count);M(list_class,"get_Item",1,Method::Item);
        M(list_class,"Contains",1,Method::Contains);M(list_class,"Add",1,Method::Add);M(list_class,"Remove",1,Method::Remove);
        M(resources,"FindObjectsOfTypeAll",1,Method::Resources);
        M(array_class,"get_Length",0,Method::Length);M(array_class,"GetValueImpl",1,Method::ArrayItem);
        library=O(library_class);profile=NewProfile();
        for(const char* field:{"skins","accessories","emotes"}) {
            view_lists[field]=O(list_class);view_lists[field]->items=profile->fields[field]->items;
        }
        for(const char* category:{"skins","accessories","emotes"}){
            library_class->fields[category]={category};
            auto* list=O(list_class);library->fields[category]=list;
            for(int i=0;i<12;++i){auto* e=O(entry_class);e->fields["Id"]=S(std::string(category)+std::to_string(i));list->items.push_back(e);}
        }
        entry_class->fields["Id"]={"Id"};
        array=O(array_class);array->items={library};type_object=O(C("System","Type"));library_type={library_class};
    }
    Object* Box(int n){auto* o=O(int_class);o->integer=n;return o;}
    Object* Box(bool b){auto* o=O(bool_class);o->boolean=b;return o;}
};
Domain* DomainGet(){return &g->domain;}
const Assembly** Assemblies(Domain*,size_t* n){*n=1;return g->assemblies;}
const Image* ImageGet(const Assembly*){return &g->image;}
Class* ClassGet(const Image*,const char* ns,const char* name){
    for(auto& c:g->classes) if(c->ns==ns&&c->name==name) return c.get();
    return nullptr;
}
Class* Parent(Class*){return nullptr;}
const Method* MethodGet(Class* c,const char* name,int args){auto it=c->methods.find({name,args});return it==c->methods.end()?nullptr:&it->second;}
Class* ObjectClass(Object* o){return o?o->klass:nullptr;}
Field* FieldGet(Class* c,const char* name){auto it=c->fields.find(name);return it==c->fields.end()?nullptr:&it->second;}
void FieldRead(Object* o,Field* f,void* out){assert(o&&f);*static_cast<Object**>(out)=o->fields[f->name];}
void StaticRead(Field* f,void* out){assert(f&&f->name=="oly");*static_cast<Object**>(out)=g->listener;}
Handle Root(Object* o,bool pinned){assert(o&&!pinned);const auto h=++g->last;g->roots[h]=o;return h;}
Object* Target(Handle h){auto it=g->roots.find(h);return it==g->roots.end()?nullptr:it->second;}
void Free(Handle h){assert(g->roots.erase(h)==1);}
String* StringNew(const char* s){return reinterpret_cast<String*>(g->S(s));}
int32_t StringLength(String* s){return static_cast<int32_t>(reinterpret_cast<Object*>(s)->text.size());}
const uint16_t* Chars(String* s){return reinterpret_cast<const uint16_t*>(reinterpret_cast<Object*>(s)->text.data());}
const Type* ClassType(Class* c){assert(c==g->library_class);return &g->library_type;}
Object* TypeObject(const Type* t){assert(t->klass==g->library_class);return g->type_object;}
const Type* FieldType(Field* f){return f->type;}
Class* ClassFromType(const Type* t){return t->klass;}
Object* ObjectNew(Class* c){return g->O(c);}
void* Unbox(Object* o){return o->klass==g->bool_class?static_cast<void*>(&o->boolean):static_cast<void*>(&o->integer);}
bool Equal(Object* a,Object* b){return a&&b&&a->text==b->text;}
Object* Invoke(const Method* m,void* instance,void** args,Object** error){
    ++g->calls;*error=nullptr;auto* self=static_cast<Object*>(instance);
    switch(m->kind){
    case Method::Library:return g->static_catalog?g->library:nullptr;
    case Method::Profile:return g->null_profile?nullptr:g->profile;
    case Method::View:return g->view_lists[m->field];
    case Method::Notify:
        assert(self==g->listener&&self->klass->name=="Action");
        ++g->notifications;
        if(g->reenter)g->reenter->Tick(1000000); // Must not recursively edit/re-notify.
        if(g->notification_error){*error=g->library;return nullptr;}
        for(auto& v:g->view_lists) {
            assert(g->profile->fields[v.first]);
            v.second->items=g->profile->fields[v.first]->items;
            // Simulate the exact live card feed being filtered/expanded independently
            // of the raw profile list. The bridge must mirror the getter result.
            if(v.first=="skins")v.second->items.push_back(g->S("wardrobe-only-skin"));
            if(v.first=="accessories")v.second->items.push_back(g->S("wardrobe-only-accessory"));
        }
        return nullptr;
    case Method::GetField:if(g->throw_getter)*error=g->library;return self->fields[m->field];
    case Method::SetField:{auto* value=static_cast<Object*>(args[0]);assert(!value||value->klass==g->list_class);self->fields[m->field]=value;return nullptr;}
    case Method::Constructor:assert(self->klass==g->list_class);self->items.clear();return nullptr;
    case Method::Count:case Method::Length:return g->Box(static_cast<int>(self->items.size()));
    case Method::Item:case Method::ArrayItem:{int index=*static_cast<int*>(args[0]);assert(index>=0&&index<static_cast<int>(self->items.size()));return self->items[index];}
    case Method::Contains:case Method::Remove:{
        auto* value=static_cast<Object*>(args[0]);assert(value&&value->klass==g->string_class);
        auto it=std::find_if(self->items.begin(),self->items.end(),[&](Object* x){return Equal(x,value);});
        bool found=it!=self->items.end();
        if(m->kind==Method::Remove&&found){self->items.erase(it);++g->removes;++g->mutations_per_tick;}
        return g->Box(found);
    }
    case Method::Add:{
        auto* value=static_cast<Object*>(args[0]);assert(value&&value->klass==g->string_class);
        ++g->adds;++g->mutations_per_tick;
        if(g->fail_add&&g->adds==g->fail_add){
            if(g->throw_after_add)self->items.push_back(value);
            *error=g->library;return nullptr;
        }
        self->items.push_back(value);return nullptr;
    }
    case Method::Resources:assert(args[0]==g->type_object);if(g->throw_resource)*error=g->library;return g->array;
    }
    assert(false);return nullptr;
}
Api API(){
    Api a{};a.domain_get=DomainGet;a.domain_get_assemblies=Assemblies;a.assembly_get_image=ImageGet;
    a.class_from_name=ClassGet;a.class_get_method_from_name=MethodGet;a.class_get_parent=Parent;
    a.object_get_class=ObjectClass;a.class_get_field_from_name=FieldGet;a.field_get_value=FieldRead;
    a.field_static_get_value=StaticRead;
    a.runtime_invoke=Invoke;a.object_unbox=Unbox;a.string_new=StringNew;a.string_length=StringLength;a.string_chars=Chars;
    a.gchandle_new=Root;a.gchandle_get_target=Target;a.gchandle_free=Free;a.class_get_type=ClassType;a.type_get_object=TypeObject;
    a.field_get_type=FieldType;a.class_from_type=ClassFromType;a.object_new=ObjectNew;return a;
}
void Tick(Controller& c,double t){g->mutations_per_tick=0;c.Tick(t);assert(g->mutations_per_tick<=8);}
void Run(Controller& c,double start=0,int frames=100){for(int i=0;i<frames;++i)Tick(c,start+i*.1);}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
int main(){
    {Fixture f;auto api=API();{Controller c(api);
        Tick(c,0);Tick(c,2.9);assert(!f.calls&&!f.adds);
        Run(c,3);assert(c.GetStatus().phase==Phase::Ready&&f.adds==36);
        // A shop can read the modified source while the wardrobe still holds its
        // previous one-skin view. Only the change event rebuilds that view.
        assert(f.notifications==1&&f.view_lists["skins"]->items.size()==14);
        assert(f.view_lists["accessories"]->items.size()==13&&f.view_lists["emotes"]->items.size()==12);
        assert(f.profile->fields["skins"]->items.size()==13);
        const auto wardrobe=wardrobe_bridge::Read();
        assert(wardrobe.items[1].size()==14&&wardrobe.items[0].size()==13);
        assert(std::find(wardrobe.items[1].begin(),wardrobe.items[1].end(),"wardrobe-only-skin")!=wardrobe.items[1].end());
        assert(std::find(wardrobe.items[0].begin(),wardrobe.items[0].end(),"wardrobe-only-accessory")!=wardrobe.items[0].end());
        assert(wardrobe_bridge::Select(1,"wardrobe-only-skin"));
        assert(!wardrobe_bridge::Select(1,"not-in-live-wardrobe"));
        assert(f.profile->fields["skins"]->items[0]->text==u"owned_skin");
        Run(c,20);assert(f.adds==36&&f.notifications==1); // No repeated notification each frame.
        const auto refreshed=wardrobe_bridge::Read();
        assert(std::find(refreshed.items[1].begin(),refreshed.items[1].end(),"wardrobe-only-skin")!=refreshed.items[1].end());
        f.view_lists["skins"]->items.clear();f.view_lists["accessories"]->items.clear();
        Run(c,32);assert(f.notifications==2&&wardrobe_bridge::Read().items[1].size()==14);
        f.profile->fields["emotes"]->items.clear();Run(c,48);assert(f.adds==48);
        f.profile=f.NewProfile();Run(c,60);assert(f.adds==84); // New sign-in cache.
    }assert(f.roots.empty());}
    for(bool after:{false,true}){Fixture f;f.fail_add=10;f.throw_after_add=after;auto api=API();{
        Controller c(api);Run(c);assert(c.GetStatus().phase==Phase::Disabled);
        assert(f.notifications==0); // Do not expose a partially committed catalogue.
        assert(f.profile->fields["skins"]->items.size()==1);
        assert(f.profile->fields["accessories"]->items.empty());assert(f.profile->fields["emotes"]->items.empty());
    }assert(f.roots.empty());}
    {Fixture f;f.null_profile=true;auto api=API();{Controller c(api);Run(c);assert(!f.adds);
        f.null_profile=false;Run(c,20);assert(c.GetStatus().phase==Phase::Ready);
    }assert(f.roots.empty());}
    {Fixture f;auto api=API();{Controller c(api);Run(c,0,33);
        for(int i=33;i<100&&!f.adds;++i)Tick(c,i*.1);
        assert(f.adds>0);auto* old=f.profile;f.profile=f.NewProfile();Run(c,10);Run(c,22);
        assert(c.GetStatus().phase==Phase::Ready&&old->fields["skins"]->items.size()==1);
        assert(f.profile->fields["skins"]->items.size()==13);
    }assert(f.roots.empty());}
    {Fixture f;f.static_catalog=false;auto api=API();{Controller c(api);Run(c);assert(c.GetStatus().phase==Phase::Ready);}assert(f.roots.empty());}
    {Fixture f;f.static_catalog=false;f.array->items.push_back(f.library);auto api=API();{
        Controller c(api);Run(c);assert(!f.adds&&c.GetStatus().phase==Phase::Waiting);
    }assert(f.roots.empty());}
    {Fixture f;auto* list=f.library->fields["skins"];list->items.resize(513,list->items[0]);auto api=API();{
        Controller c(api);Run(c);assert(!f.adds&&c.GetStatus().phase==Phase::Waiting);
    }assert(f.roots.empty());}
    {Fixture f;f.library->fields["skins"]->items[0]->fields["Id"]=f.library;auto api=API();{
        Controller c(api);Run(c);assert(!f.adds);
    }assert(f.roots.empty());}
    {Fixture f;auto* list=f.library->fields["skins"];list->items.push_back(list->items[0]);auto api=API();{
        Controller c(api);Run(c);assert(c.GetStatus().phase==Phase::Ready&&f.adds==36);
    }assert(f.roots.empty());}
    {Fixture f;auto api=API();api.field_get_value=nullptr;{Controller c(api);Run(c);assert(!f.calls&&!f.adds&&c.GetStatus().phase==Phase::Disabled);}assert(f.roots.empty());}
    {Fixture f;for(auto& p:f.profile->fields)p.second=nullptr;auto api=API();{
        Controller c(api);Run(c);assert(c.GetStatus().phase==Phase::Ready&&f.adds==36);
        for(auto& p:f.profile->fields)assert(p.second&&p.second->items.size()==12);
    }assert(f.roots.empty());}
    {Fixture f;auto* original=f.profile->fields["skins"];f.throw_getter=true;auto api=API();{
        Controller c(api);Run(c);assert(!f.adds&&f.profile->fields["skins"]==original&&original->items.size()==1);
    }assert(f.roots.empty());}
    {Fixture f;auto* list=f.library->fields["skins"];list->items.push_back(nullptr);
        auto* empty=f.O(f.entry_class);list->items.push_back(empty);auto api=API();{
        Controller c(api);Run(c);assert(c.GetStatus().phase==Phase::Ready&&f.adds==36);
    }assert(f.roots.empty());}
    {Fixture f;f.listener=nullptr;auto api=API();{Controller c(api);Run(c);assert(c.GetStatus().phase==Phase::Ready&&f.notifications==0);}assert(f.roots.empty());}
    {Fixture f;f.listener=f.S("not an Action");auto api=API();{Controller c(api);Run(c);assert(c.GetStatus().phase==Phase::Ready&&f.notifications==0);}assert(f.roots.empty());}
    {Fixture f;f.notification_error=true;auto api=API();{Controller c(api);Run(c);assert(c.GetStatus().phase==Phase::Ready&&f.notifications==2);f.notification_error=false;Run(c,20);assert(f.notifications==3&&wardrobe_bridge::Read().items[1].size()==14);}assert(f.roots.empty());}
    {Fixture f;auto api=API();{Controller c(api);f.reenter=&c;Run(c);assert(c.GetStatus().phase==Phase::Ready&&f.notifications==1);}assert(f.roots.empty());}
    {Fixture f;auto api=API();api.field_static_get_value=nullptr;{Controller c(api);Run(c);assert(c.GetStatus().phase==Phase::Ready&&f.notifications==0);}assert(f.roots.empty());}
    std::cout<<"Cosmetic catalog: wardrobe change notification after complete commit, stale-view rebuild, no partial/repeated notification, listener failure/reentry guards, bounded edits, rollback, profile refresh and GC roots: PASS\n";
}
