#include "game_text_rules.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <map>
namespace il2cpp {
struct Class { const char* ns; const char* name; };
struct Object { Class* klass; std::string pattern; };
struct String { std::string value; };
struct Method {};
struct Domain {}; struct Assembly {}; struct Image {};
struct Field { Object* value; const char* name; };
}
using namespace il2cpp;
namespace {
Class regex{"System.Text.RegularExpressions","Regex"},input{"","TMPInputFieldTagFilter"},display{"","TMPTextTagFilter"};
Object original_input{&regex,"original input filter"},original_display{&regex,"original display filter"},replacement{&regex,""};
Field input_field{&original_input,"qfy"},display_field{&original_display,"qfz"};
Method constructor; Domain domain; Assembly assembly; Image image; const Assembly* assemblies[]={&assembly};
std::map<Handle,Object*> handles;Handle last_handle=0;
bool constructor_error=false,missing_display=false,corrupt_once=false,ignore_writes=false;int writes=0,roots=0,initializations=0;
Domain* DomainGet(){return &domain;}
const Assembly** Assemblies(Domain*,size_t* count){*count=1;return assemblies;}
const Image* ImageGet(const Assembly*){return &image;}
Class* Find(const Image*,const char* ns,const char* name){
 for(auto* klass:{&regex,&input,&display}) if(!std::strcmp(ns,klass->ns)&&!std::strcmp(name,klass->name)) return missing_display&&klass==&display?nullptr:klass;
 return nullptr;
}
const Method* MethodGet(Class* klass,const char* name,int count){return klass==&regex&&count==1&&!std::strcmp(name,".ctor")?&constructor:nullptr;}
Class* Parent(Class*){return nullptr;}
Object* Invoke(const Method*,void* self,void** args,Object** error){
 *error=constructor_error?&original_input:nullptr;
 if(!*error) static_cast<Object*>(self)->pattern=static_cast<String*>(args[0])->value;
 return nullptr;
}
Class* ObjectClass(Object* object){return object?object->klass:nullptr;}
Object* New(Class* klass){assert(klass==&regex);return &replacement;}
String* NewString(const char* value){static String string;string.value=value;return &string;}
Handle Root(Object* object,bool){++roots;const auto h=++last_handle;handles[h]=object;return h;}
Object* RootGet(Handle h){auto it=handles.find(h);return it==handles.end()?nullptr:it->second;}
void Free(Handle h){assert(handles.erase(h)==1);--roots;}
Field* FieldGet(Class* klass,const char* name){
 if(klass==&input&&!std::strcmp(name,"qfy")) return &input_field;
 if(klass==&display&&!std::strcmp(name,"qfz")) return &display_field;
 return nullptr;
}
void FieldRead(Field* field,void* output){*static_cast<Object**>(output)=field->value;}
// Matches Unity 6 Field::StaticSetValue -> SetValueRaw(...,false): reference value is direct.
void FieldWrite(Field* field,void* value){
 ++writes;
 if(ignore_writes)return;
 if(corrupt_once){corrupt_once=false;field->value=reinterpret_cast<Object*>(0x1234);return;}
 field->value=static_cast<Object*>(value);
}
bool Managed(Object* object){return object==&original_input||object==&original_display||object==&replacement;}
bool HeapReferencesValid(){return Managed(input_field.value)&&Managed(display_field.value);}
void Init(Class*){++initializations;}
Api Fake(){Api a{};a.domain_get=DomainGet;a.domain_get_assemblies=Assemblies;a.assembly_get_image=ImageGet;a.class_from_name=Find;
 a.class_get_method_from_name=MethodGet;a.class_get_parent=Parent;a.runtime_invoke=Invoke;a.object_get_class=ObjectClass;
 a.object_new=New;a.string_new=NewString;a.gchandle_new=Root;a.gchandle_get_target=RootGet;a.gchandle_free=Free;
 a.class_get_field_from_name=FieldGet;a.field_static_get_value=FieldRead;a.field_static_set_value=FieldWrite;a.runtime_class_init=Init;
 return a;
}
void Reset(){handles.clear();last_handle=0;writes=roots=initializations=0;constructor_error=missing_display=corrupt_once=ignore_writes=false;input_field.value=&original_input;display_field.value=&original_display;}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
int main(){
 // Reproduce the old bug without dereferencing its dangling native-stack pointer.
 Reset();Object* local=&replacement;FieldWrite(&input_field,&local);
 assert(input_field.value==reinterpret_cast<Object*>(&local)&&!HeapReferencesValid());
 Reset();auto api=Fake();auto s=game_text_rules::Install(api);
 assert(s.input_filter&&s.display_filter&&!s.username_limit&&writes==2&&roots==0&&initializations==2);
 assert(HeapReferencesValid());
 assert(replacement.pattern=="(?!)"&&input_field.value==&replacement&&display_field.value==&replacement);
 Reset();constructor_error=true;s=game_text_rules::Install(api);assert(!s.input_filter&&!s.display_filter&&writes==0&&roots==0);
 Reset();missing_display=true;s=game_text_rules::Install(api);assert(s.input_filter&&!s.display_filter&&writes==1&&roots==0);
 Reset();corrupt_once=true;s=game_text_rules::Install(api);
 assert(!s.input_filter&&s.display_filter&&input_field.value==&original_input&&HeapReferencesValid()&&roots==0&&writes==3);
 Reset();ignore_writes=true;s=game_text_rules::Install(api);
 assert(!s.input_filter&&!s.display_filter&&HeapReferencesValid()&&roots==0&&writes==4);
 Reset();api.field_static_set_value=nullptr;s=game_text_rules::Install(api);assert(!s.input_filter&&!s.display_filter&&writes==0);
 std::cout<<"Game filters: actual IL2CPP reference ABI, old stack-pointer corruption reproduced, verified writes/rollback, GC roots: PASS\n";
}
