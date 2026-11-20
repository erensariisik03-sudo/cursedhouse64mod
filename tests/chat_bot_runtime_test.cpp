#include "chat_bot.h"
#include <cassert>
#include "utf_conversion.h"
#include <condition_variable>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
namespace il2cpp {
struct Method { std::string owner,name; };
struct Field { std::string name; };
struct Class { std::string ns,name;std::map<std::pair<std::string,int>,Method> methods;std::map<std::string,Field> fields; };
struct Type { Class* klass; };
struct Object { Class* klass=nullptr;std::u16string text;bool flag=true;int number=0;Class* reflection=nullptr;std::vector<Object*> items;Object* label=nullptr; };
struct String:Object{};struct Domain{};struct Assembly{};struct Image{};
}
#include "../src/chat_bot.cpp"
using namespace il2cpp;
namespace {
struct Fake;
Fake* fake;
std::atomic<int> http_calls{0};
std::mutex http_mutex;std::condition_variable http_cv;bool hold_http=false;
std::string http_reply="Ali, bu gerçek bir cevap.";
struct Fake {
 Domain domain;Assembly assembly;Image image;const Assembly* assemblies[1]={&assembly};
 std::map<std::pair<std::string,std::string>,std::unique_ptr<Class>> classes;
 std::vector<std::unique_ptr<Object>> objects;std::map<Class*,Type> types;std::map<Handle,Object*> roots;Handle next=0;
 Object *current_room{},*manager{},*input{};Class *messages{},*managers{},*strings{},*arrays{},*players{};
 std::vector<Object*> chat_lines,others;std::vector<std::string> sent;
 bool allowed=true,focused=false,panel=false;std::string own="Bot";
 Class* C(const std::string& ns,const std::string& name){auto& c=classes[{ns,name}];if(!c){c=std::make_unique<Class>();c->ns=ns;c->name=name;}return c.get();}
 Object* O(Class* c){auto o=std::make_unique<Object>();o->klass=c;auto* p=o.get();objects.push_back(std::move(o));return p;}
 Object* S(const std::string& s){auto* o=O(strings);o->text=test_utf::FromUtf8(s);return o;}
 Object* B(bool b){auto* o=O(C("System","Boolean"));o->flag=b;return o;}
 Object* I(int n){auto* o=O(C("System","Int32"));o->number=n;return o;}
 Fake(){
  fake=this;chat_bot_bridge::Take();chat_bot_bridge::Publish({});http_calls=0;
  strings=C("System","String");arrays=C("System","Array");players=C("Photon.Realtime","Player");
  messages=C("","ChatMessage");managers=C("","ChatManager");manager=O(managers);input=O(C("TMPro","TMP_InputField"));
  current_room=O(C("Photon.Realtime","Room"));others={S("Ali"),S("Veli")};
 }
 void Message(const std::string& body){auto* m=O(messages);m->number=static_cast<int>(chat_lines.size()+1);m->label=S("Ali: "+body);chat_lines.push_back(m);}
};
Domain* DomainGet(){return &fake->domain;}
const Assembly** Assemblies(Domain*,size_t* count){*count=1;return fake->assemblies;}
const Image* ImageGet(const Assembly*){return &fake->image;}
Class* ClassGet(const Image*,const char* ns,const char* name){return fake->C(ns,name);}
Class* Parent(Class*){return nullptr;}
const Method* MethodGet(Class* c,const char* name,int count){auto& m=c->methods[{name,count}];m={c->name,name};return &m;}
Field* FieldGet(Class* c,const char* name){auto& f=c->fields[name];f.name=name;return &f;}
const Type* ClassType(Class* c){return &(fake->types[c]={c});}
Object* TypeObject(const Type* t){auto* o=fake->O(fake->C("System","Type"));o->reflection=t->klass;return o;}
Handle MakeRoot(Object* o,bool){const auto h=++fake->next;fake->roots[h]=o;return h;}
Object* GetRoot(Handle h){return fake->roots.at(h);}
void FreeRoot(Handle h){fake->roots.erase(h);}
void ReadField(Object* o,Field* field,void* out){
 if(field->name=="myMessage")*static_cast<Object**>(out)=o->label;
 else if(field->name=="inputField")*static_cast<Object**>(out)=fake->input;
 else if(field->name=="canSendMessage")*static_cast<bool*>(out)=fake->allowed;
 else assert(false);
}
String* StringNew(const char* s){return reinterpret_cast<String*>(fake->S(s));}
int32_t StringLength(String* s){return static_cast<int32_t>(s->text.size());}
const uint16_t* StringChars(String* s){return reinterpret_cast<const uint16_t*>(s->text.data());}
void* Unbox(Object* o){if(o->klass->name=="Boolean")return &o->flag;return &o->number;}
Object* Invoke(const Method* m,void* self,void** args,Object** error){
 *error=nullptr;auto* o=static_cast<Object*>(self);const auto& name=m->name;
 if(name=="FindObjectsByType"){
  auto* list=fake->O(fake->arrays);auto* type=static_cast<Object*>(args[0]);
  if(type->reflection==fake->messages)list->items=fake->chat_lines;
  else if(type->reflection==fake->managers)list->items={fake->manager};
  return list;
 }
 if(name=="get_Length")return fake->I(static_cast<int>(o->items.size()));
 if(name=="GetValueImpl")return o->items.at(*static_cast<int*>(args[0]));
 if(name=="op_Implicit")return fake->B(args[0]!=nullptr);
 if(name=="get_isActiveAndEnabled"||name=="get_IsMessageQueueRunning")return fake->B(true);
 if(name=="GetInstanceID"||name=="GetSiblingIndex")return fake->I(o->number);
 if(name=="get_transform")return o;
 if(name=="get_CurrentRoom")return fake->current_room;
 if(name=="get_PlayerListOthers"){auto* list=fake->O(fake->arrays);list->items=fake->others;return list;}
 if(name=="get_NickName")return o?o:fake->S(fake->own);
 if(name=="get_isFocused")return fake->B(fake->focused);
 if(name=="get_text")return fake->S(test_utf::ToUtf8(o->text));
 if(name=="set_text"){o->text=static_cast<Object*>(args[0])->text;return nullptr;}
 if(name=="SendMessage"){
  assert(fake->allowed&&!fake->focused);
  const auto text=test_utf::ToUtf8(fake->input->text);assert(!chat_bot_core::IsSkipReply(text,"Ali"));
  fake->sent.push_back(text);fake->input->text.clear();return nullptr;
 }
 assert(false&&"Unexpected managed call");return nullptr;
}
Api MakeApi(){Api a{};a.domain_get=DomainGet;a.domain_get_assemblies=Assemblies;a.assembly_get_image=ImageGet;a.class_from_name=ClassGet;
 a.class_get_method_from_name=MethodGet;a.class_get_parent=Parent;a.class_get_field_from_name=FieldGet;a.class_get_type=ClassType;a.type_get_object=TypeObject;
 a.gchandle_new=MakeRoot;a.gchandle_get_target=GetRoot;a.gchandle_free=FreeRoot;a.field_get_value=ReadField;
 a.runtime_invoke=Invoke;a.object_unbox=Unbox;a.string_new=StringNew;a.string_length=StringLength;a.string_chars=StringChars;return a;}
void Response(const std::string& text){std::lock_guard<std::mutex> l(http_mutex);http_reply=text;}
struct Harness {
 Fake f;Api api;chat_bot::Controller bot;double now=104.5;
 Harness():api(MakeApi()),bot(api){bot.Tick(100);chat_bot_bridge::BeginEdit(chat_bot_bridge::Edit::Key);bot.Tick(104);
  assert(chat_bot_bridge::Read().chat_ready&&chat_bot_bridge::Read().has_key);
  chat_bot_bridge::ToggleAll();chat_bot_bridge::Toggle();bot.Tick(now);assert(chat_bot_bridge::Read().enabled);}
 void Tick(double at){now=at;bot.Tick(now);assert(f.roots.empty());}
 void Step(){Tick(now+.5);}
 void Message(const std::string& s){f.Message(s);Step();}
 void Drain(){for(int i=0;i<8;++i){Step();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  for(int i=0;i<400;++i){Step();if(!chat_bot_bridge::Read().busy)return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}assert(false&&"HTTP worker timeout");}
};
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
namespace keyboard_bridge {
void OpenNative(const char*,bool){fake->panel=true;}
bool PollNative(char* text,size_t capacity,bool& submit,bool& visible){if(!fake->panel)return false;std::snprintf(text,capacity,"TEST_KEY");submit=visible=true;return true;}
void CloseNative(){fake->panel=false;}
}
namespace chat_bot_http {
bool Post(const std::string&,const std::string&,const std::string&,int& code,std::string& response){
 ++http_calls;std::unique_lock<std::mutex> lock(http_mutex);http_cv.wait(lock,[]{return !hold_http;});
 code=200;response="{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\""+chat_bot_core::JsonEscape(http_reply)+"\"}]}}]}";return true;
}
}
int main(){
 {Harness h;
  for(const char* greeting:{"merhaba","slm","selam","sa","nasılsınız","Bot merhaba"}){
   h.Tick(h.now+31);const auto before=h.f.sent.size();h.Message(greeting);h.Step();
   assert(h.f.sent.size()==before+1&&http_calls==0);
  }
  const auto count=h.f.sent.size();h.Message("merhaba");h.Step();assert(h.f.sent.size()==count&&http_calls==0);
  Response("Ali, dünya kütleçekim nedeniyle yuvarlaktır.");h.Message("Bot merhaba, dünya neden yuvarlak?");h.Drain();h.Step();
  assert(http_calls==1&&h.f.sent.back().find("kütleçekim")!=std::string::npos);
  const int requests=http_calls;h.Message("Veli merhaba");h.Step();assert(http_calls==requests);
  h.Message("asdasdasdasd");h.Step();assert(http_calls==requests);
 }
 {Harness h;
  h.Message("Bot seni sikeyim");assert(h.f.sent.size()==1&&http_calls==0);
  h.Message("Bot seni sikeyim");for(int i=0;i<5;++i)h.Step();
  assert(h.f.sent.size()==2&&http_calls==0);
  assert(h.f.sent[0].find(u8"saygılı")!=std::string::npos);
  const auto warnings=h.f.sent.size();h.Message("Bot siktir git");h.Step();assert(h.f.sent.size()==warnings&&http_calls==0);
  Response("Ali, oyun ayarlarını kontrol edebilirsin.");h.Message("Bot amk oyun nasıl oynanır?");h.Drain();h.Step();
  assert(http_calls==1&&h.f.sent.back().find("ayarlarını")!=std::string::npos);
  h.Tick(h.now+7);h.Message("Bot siktir kelimesi ne demek?");h.Drain();h.Step();assert(http_calls==2);
  const int requests=http_calls.load();h.Message("Veli seni sikeyim");h.Step();assert(http_calls==requests);
 }
 {Harness h;
  for(const char* text:{"zekalı insan neden böyle", "yapaylık var", "yapayzekanın cevabı nedir"}){h.Message(text);h.Step();}
  assert(http_calls==0);
  Response("Ali, sorunu yanıtlıyorum.");int expected=0;
  for(const char* text:{"zeka neden böyle", "yapay bunu açıkla", "YAPAYZEKA bir soru soruyorum"}){
   h.Tick(h.now+7);h.Message(text);h.Drain();h.Step();assert(http_calls==++expected);
  }
 }
 {Harness h;chat_bot_bridge::ToggleStrict();h.Step();
  for(const char* control:{"SKIP","Ali: SKIP","Ali, **SKIP**","Ali SKIP",u8"SKİP"}){
   Response(control);h.Tick(h.now+7);const auto sent=h.f.sent.size();h.Message("Bot bu mesaja cevap verme");h.Drain();h.Step();assert(h.f.sent.size()==sent);
  }
  // A marker arriving as a later chat chunk is also blocked at TrySend.
  Response("Ali, "+std::string(190,'a')+". SKIP");h.Tick(h.now+7);const auto sent=h.f.sent.size();
  h.Message("Bot uzun cevap ver");h.Drain();for(int i=0;i<8;++i)h.Step();assert(h.f.sent.size()==sent+1);
 }
 {Harness h;
  h.Tick(119);assert(h.f.sent.empty());h.Tick(119.5);assert(h.f.sent.empty());h.Tick(120);
  assert(h.f.sent.size()==1&&h.f.sent[0].find("\"Bot\"")!=std::string::npos&&http_calls==0);
  h.Tick(239);assert(h.f.sent.size()==1);h.Tick(239.5);h.Tick(240);assert(h.f.sent.size()==2);
  chat_bot_bridge::ToggleStrict();h.Step();h.Tick(500);assert(h.f.sent.size()==2);
  chat_bot_bridge::ToggleStrict();h.Tick(501);h.f.focused=true;h.Tick(516);h.Step();assert(h.f.sent.size()==2);
  chat_bot_bridge::ToggleStrict();h.Step();h.f.focused=false;h.Step();assert(h.f.sent.size()==2); // queued hint cancelled
  chat_bot_bridge::ToggleStrict();h.Tick(520);h.f.input->text=u"my draft";h.Tick(535);h.Step();
  assert(h.f.input->text==u"my draft"&&h.f.sent.size()==2);
  h.f.current_room=nullptr;h.Step();h.f.input->text.clear();h.Step();assert(!chat_bot_bridge::Read().enabled&&h.f.sent.size()==2);
 }
 {Harness h;h.f.others.clear();h.Tick(130);h.Step();assert(h.f.sent.empty()&&http_calls==0);
  h.f.others={h.f.S("Ali")};h.f.own.clear();h.Tick(200);h.Step();assert(h.f.sent.empty());
  h.f.own="NewBot";h.Tick(201);h.f.allowed=false;h.Tick(216);h.Step();assert(h.f.sent.empty());
  h.f.own="RenamedBot";h.Tick(217);h.f.allowed=true;h.Step();assert(h.f.sent.empty());
  h.Tick(232);h.Step();assert(h.f.sent.size()==1&&h.f.sent[0].find("RenamedBot")!=std::string::npos);
 }
 {Harness h;chat_bot_bridge::ToggleStrict();h.Step();
  {std::lock_guard<std::mutex> lock(http_mutex);hold_http=true;http_reply="Ali, eski yanıt.";}
  h.Message("Bot bir soru soruyorum");
  for(int i=0;i<400&&http_calls==0;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));
  assert(http_calls==1);
  chat_bot_bridge::ToggleStrict();h.Step(); // invalidate while the network request is still running
  {std::lock_guard<std::mutex> lock(http_mutex);hold_http=false;}http_cv.notify_all();h.Drain();h.Step();assert(h.f.sent.empty());
 }
 std::cout<<"Bot controller: zero HTTP for small talk, silent name-prefixed/chunk SKIP, timed strict-only reminders, draft/focus/cooldown/room/name guards and stale-result rejection; local profanity warnings with slang bypass and repeat bounds: PASS\n";
}
