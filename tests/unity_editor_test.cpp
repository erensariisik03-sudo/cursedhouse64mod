#define MM_AUTO_UNITY 0
#include "../src/unity_keyboard.cpp"
#include <cassert>
#include <iostream>
#include <string>
#include <map>
#include <codecvt>
#include <locale>
namespace il2cpp { struct String { std::u16string value; }; }
namespace {
enum Tag : uintptr_t { Text=1,SetText=2,Hide=3,SetHideTag=4,Focused=5,ReadOnly=6,Interactable=7,InputType=8,Deactivate=9,Submit=10,AliveTag=11,Limit=12,SetLimitTag=13,FindChat,ArrayLength,ArrayItem,Active,ChatSend };
const Method* M(Tag t){return reinterpret_cast<const Method*>(t);}
struct Editor { std::string text="old"; int limit=25; bool hidden=false,focused=false; } editor;
Object* live=nullptr;bool visible=false,rewrite=false,blur_on_change=false,fail_limit=false;
std::string ui;bool send=false;int changes=0,submits=0,closed=0,freed=0;
bool quick=false,has_chat=true,cooldown=false,chat_blur=false,chat_disable=false,new_draft=false,chat_exception=false;
bool duplicate_chat=false,matching_field=true;
int chat_sends=0;std::string sent;
Object *chat=reinterpret_cast<Object*>(0x100),*reflection=reinterpret_cast<Object*>(0x200),*array=reinterpret_cast<Object*>(0x300);
std::map<Handle,Object*> roots;Handle next_root=0;
std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> utf;
String returned;
Object* Box(bool value){static bool box;box=value;return reinterpret_cast<Object*>(&box);}
Object* Box(int value){static int box;box=value;return reinterpret_cast<Object*>(&box);}
String* NewString(const char* text){static String s;s.value=utf.from_bytes(text);return &s;}
int32_t Length(String* s){return static_cast<int32_t>(s->value.size());}
const uint16_t* Chars(String* s){return reinterpret_cast<const uint16_t*>(s->value.data());}
void OriginalActivate(Object*,const Method*){editor.focused=true;}
Object* FakeInvoke(const Method* method,void*,void** args,Object** error){
 *error=nullptr;
 switch(static_cast<Tag>(reinterpret_cast<uintptr_t>(method))){
 case Text:returned.value=utf.from_bytes(editor.text);return reinterpret_cast<Object*>(&returned);
 case SetText:{auto* s=static_cast<String*>(args[0]);editor.text=utf.to_bytes(s->value);++changes;
  if(rewrite){const auto at=editor.text.find("<size=32>");if(at!=std::string::npos)editor.text.erase(at,9);}
  if(blur_on_change){editor.focused=false;End();}
  return nullptr;}
 case Hide:return Box(editor.hidden);
 case SetHideTag:editor.hidden=*static_cast<bool*>(args[0]);return nullptr;
 case Focused:return Box(editor.focused);
 case ReadOnly:return Box(false);
 case Interactable:return Box(true);
 case InputType:return Box(0);
 case Deactivate:editor.focused=false;return nullptr;
 case Submit:++submits;return nullptr;
 case AliveTag:return Box(args&&args[0]);
 case Limit:return Box(editor.limit);
 case SetLimitTag:if(fail_limit){fail_limit=false;*error=reinterpret_cast<Object*>(1);}else editor.limit=*static_cast<int*>(args[0]);return nullptr;
 case FindChat:assert(*static_cast<int*>(args[1])==0&&*static_cast<int*>(args[2])==0);return array;
 case ArrayLength:return Box(has_chat?(duplicate_chat?2:1):0);
 case ArrayItem:return chat;
 case Active:return Box(true);
 case ChatSend:
  if(chat_exception){*error=chat;return nullptr;}
  ++chat_sends;sent=editor.text;editor.text.clear();
  if(new_draft)ui="newer draft";
  if(chat_blur){editor.focused=false;Deselect(reinterpret_cast<Object*>(&editor),nullptr,nullptr);}
  if(chat_disable)Disable(reinterpret_cast<Object*>(&editor),nullptr);
  return nullptr;
 }
 return nullptr;
}
Handle Root(Object* self,bool){if(self==reinterpret_cast<Object*>(&editor))live=self;auto h=++next_root;roots[h]=self;return h;}
Object* TargetRoot(Handle h){return roots.at(h);}
void FreeRoot(Handle h){if(roots.at(h)==reinterpret_cast<Object*>(&editor))live=nullptr;roots.erase(h);++freed;}
const Type* ClassType(Class*){return reinterpret_cast<const Type*>(1);}
Object* TypeObject(const Type*){return reflection;}
void FieldValue(Object* self,Field* field,void* out){assert(self==chat);if(field==m.chatInput)*static_cast<Object**>(out)=matching_field?reinterpret_cast<Object*>(&editor):nullptr;else *static_cast<bool*>(out)=!cooldown;}
void NoEvent(Object*,Object*,const Method*){}
void NoUnary(Object*,const Method*){}
void* Unbox(Object* self){return self;}
void Reset(){assert(roots.empty());editor=Editor{};visible=rewrite=blur_on_change=fail_limit=send=quick=false;ui.clear();changes=submits=closed=freed=0;live=nullptr;has_chat=matching_field=true;cooldown=chat_blur=chat_disable=new_draft=chat_exception=duplicate_chat=false;chat_sends=0;sent.clear();}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
extern "C" int DobbyHook(void*,void*,void**){return -1;}
extern "C" EGLContext eglGetCurrentContext(){return nullptr;}
extern "C" EGLSurface eglGetCurrentSurface(EGLint){return nullptr;}
extern "C" EGLBoolean eglQueryContext(EGLDisplay,EGLContext,EGLint,EGLint*){return 0;}
extern "C" EGLBoolean eglQuerySurface(EGLDisplay,EGLSurface,EGLint,EGLint*){return 0;}
extern "C" EGLBoolean eglSwapBuffers(EGLDisplay,EGLSurface){return 1;}
namespace chat_bot_http {
void SetVM(JavaVM*) {}
bool Post(const std::string&,const std::string&,const std::string&,int& code,std::string&) { code=0; return false; }
}
namespace keyboard_bridge {
void OpenNative(const char*,bool) {}
bool PollNative(char*,size_t,bool&,bool&) { return false; }
void CloseNative() {}
void SurfaceFrame(int,int){}
bool RenderReady(){return true;}
void Open(const char* text,bool){ui=text;visible=true;}
void Close(){visible=false;++closed;}
void ReconcileText(const char* expected,const char* actual){if(ui==expected)ui=actual;}
bool ConsumeEmojiRequest(){return false;}
void SetEmojiSupport(uint64_t,bool){}
void Snapshot(char* text,size_t capacity,bool& submit,bool& shown,bool& quick_send){std::snprintf(text,capacity,"%s",ui.c_str());submit=send;send=false;shown=visible;quick_send=quick;quick=false;}
bool Pointer(float,float,bool){return false;}
}
int main(){
 g_api.runtime_invoke=FakeInvoke;g_api.object_unbox=Unbox;g_api.string_new=NewString;g_api.string_length=Length;g_api.string_chars=Chars;
 g_api.gchandle_new=Root;g_api.gchandle_get_target=TargetRoot;g_api.gchandle_free=FreeRoot;
 g_api.class_get_type=ClassType;g_api.type_get_object=TypeObject;g_api.field_get_value=FieldValue;
 m.text=M(Text);m.setText=M(SetText);m.hide=M(Hide);m.setHide=M(SetHideTag);m.focused=M(Focused);m.readOnly=M(ReadOnly);m.interactable=M(Interactable);
 m.inputType=M(InputType);m.deactivate=M(Deactivate);m.submit=M(Submit);m.alive=M(AliveTag);m.limit=M(Limit);m.setLimit=M(SetLimitTag);
 m.chatClass=reinterpret_cast<Class*>(1);m.chatInput=reinterpret_cast<Field*>(1);m.chatAllowed=reinterpret_cast<Field*>(2);
 m.findChat=M(FindChat);m.arrayLength=M(ArrayLength);m.arrayItem=M(ArrayItem);m.active=M(Active);m.chatSend=M(ChatSend);
 g_activate=OriginalActivate;g_deselect=NoEvent;g_disable=NoUnary;g_ready=true;g_input_ok=true;
 auto* self=reinterpret_cast<Object*>(&editor);
 Reset();Activate(self,nullptr);assert(g_editing&&visible&&editor.limit==0&&editor.hidden&&live);
 ui="<size=32><#F00F>hello";rewrite=true;PumpEditor();
 assert(editor.text=="<#F00F>hello"&&ui==editor.text&&g_applied==editor.text&&changes==1);
 PumpEditor();assert(changes==1);
 rewrite=false;ui=std::string(1024,'a');PumpEditor();assert(editor.text.size()==1024&&editor.limit==0);
 send=true;PumpEditor();assert(submits==1&&!g_editing&&!visible&&editor.limit==25&&!editor.hidden&&!live&&freed==1);
 Reset();Activate(self,nullptr);blur_on_change=true;ui="another";PumpEditor();
 assert(!g_editing&&!live&&editor.limit==25&&!editor.hidden&&freed==1);
 Reset();fail_limit=true;Activate(self,nullptr);
 assert(!g_editing&&!live&&editor.limit==25&&!editor.hidden&&editor.focused&&freed==1);
 for(int scenario=0;scenario<9;++scenario){
  Reset();Activate(self,nullptr);ui=chat_shortcut::kPayload;quick=true;
  if(scenario==1)chat_blur=true;
  if(scenario==2)new_draft=true;
  if(scenario==3)has_chat=false;
  if(scenario==4)cooldown=true;
  if(scenario==5)chat_disable=true;
  if(scenario==6)chat_exception=true;
  if(scenario==7)duplicate_chat=true;
  if(scenario==8)matching_field=false;
  PumpEditor();
  assert(!submits&&!quick);
  if(scenario==3||scenario==4||scenario==6||scenario==7||scenario==8){assert(!chat_sends&&editor.text==chat_shortcut::kPayload&&visible);}
  else {assert(chat_sends==1&&sent==u8"<size=7000>█");
   if(scenario==5)assert(!visible&&!g_target&&roots.empty());
   else {assert(visible&&editor.focused&&editor.text.empty());assert(ui==(scenario==2?"newer draft":""));}
  }
  const auto sends_before=chat_sends;PumpEditor();assert(chat_sends==sends_before);
  if(g_target)End();
  assert(roots.empty());
 }
 std::cout<<"Unity editor: active limit bypass/restoration, callbacks and submit retained, actual text readback, reentrant blur and fallback: PASS\n";
 std::cout<<"Chat shortcut: exact U+2588/no quotes, normal send once, cooldown/owner checks, keyboard refocus, new draft preservation, exceptions and scene-disable roots: PASS\n";
}
