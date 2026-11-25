#define MM_AUTO_UNITY 0
#include "../src/unity_keyboard.cpp"
#include <cassert>
#include <iostream>
#include <map>
#include "utf_conversion.h"
namespace il2cpp { struct String { std::u16string value; }; }
namespace {
enum Tag : uintptr_t { Text=1,SetText,Hide,SetHideTag,Focused,ReadOnly,Interactable,InputType,Deactivate,SubmitTag,AliveTag,Limit,SetLimitTag,FindChat,ArrayLength,ArrayItem,Active,ChatSend,SelectionAnchor,SelectionFocus,SetAnchor,SetFocus,Keyboard,CanGetSelection,CanSetSelection,KeyboardSelection,SetKeyboardSelection,ScreenHeight,KeyboardArea };
const Method* M(Tag t){return reinterpret_cast<const Method*>(t);}
struct Editor {
 std::string text="old",ime="old";int limit=25,cursor=3,anchor=3,input_type=0;
 bool hidden=false,focused=false,native_open=false,pending_native=false;
} editor;
bool visible=false,rewrite=false,blur_on_change=false,fail_limit=false,fail_set=false;
bool render_ready=true,selection_supported=true;
bool has_chat=true,cooldown=false,chat_blur=false,chat_disable=false,new_draft=false,chat_exception=false;
bool duplicate_chat=false,matching_field=true;
int changes=0,notifications=0,submits=0,chat_sends=0,opened_limit=-1;
std::string sent,notified;
Object *chat=reinterpret_cast<Object*>(0x100),*reflection=reinterpret_cast<Object*>(0x200),*array=reinterpret_cast<Object*>(0x300),*keyboard=reinterpret_cast<Object*>(0x400);
std::map<Handle,Object*> roots;Handle next_root=0;
String returned;
Object* Box(bool value){static bool box;box=value;return reinterpret_cast<Object*>(&box);}
Object* Box(int value){static int box;box=value;return reinterpret_cast<Object*>(&box);}
String* NewString(const char* text){static String s;s.value=test_utf::FromUtf8(text);return &s;}
int32_t Length(String* s){return static_cast<int32_t>(s->value.size());}
const uint16_t* Chars(String* s){return reinterpret_cast<const uint16_t*>(s->value.data());}
Object* Self(){return reinterpret_cast<Object*>(&editor);}
void OriginalActivate(Object*,const Method*){
 editor.focused=true;editor.native_open=!editor.hidden;opened_limit=editor.limit;
}
void OriginalChanged(Object*,const Method*){
 ++notifications;notified=editor.text;
 assert(text_buffer::Count(notified.c_str())<=1024);
 if(blur_on_change){editor.focused=false;End();}
}
void OriginalSubmit(Object*,const Method*){++submits;assert(text_buffer::Count(editor.text.c_str())<=1024);}
void OriginalLate(Object*,const Method*){
 if(editor.pending_native){editor.pending_native=false;editor.text=editor.ime;Changed(Self(),nullptr);}
}
Object* FakeInvoke(const Method* method,void*,void** args,Object** error){
 *error=nullptr;
 switch(static_cast<Tag>(reinterpret_cast<uintptr_t>(method))){
 case Text:returned.value=test_utf::FromUtf8(editor.text);return reinterpret_cast<Object*>(&returned);
 case SetText:{
  if(fail_set){fail_set=false;*error=chat;return nullptr;}
  auto* s=static_cast<String*>(args[0]);editor.text=test_utf::ToUtf8(s->value);++changes;
  if(rewrite){const auto at=editor.text.find("<size=32>");if(at!=std::string::npos)editor.text.erase(at,9);}
  editor.ime=editor.text;Changed(Self(),nullptr);return nullptr;
 }
 case Hide:return Box(editor.hidden);
 case SetHideTag:editor.hidden=*static_cast<bool*>(args[0]);return nullptr;
 case Focused:return Box(editor.focused);
 case ReadOnly:return Box(false);
 case Interactable:return Box(true);
 case InputType:return Box(editor.input_type);
 case Deactivate:editor.focused=false;editor.native_open=false;return nullptr;
 case SubmitTag:Submit(Self(),nullptr);return nullptr;
 case AliveTag:return Box(args&&args[0]);
 case Limit:return Box(editor.limit);
 case SetLimitTag:if(fail_limit){fail_limit=false;*error=chat;}else editor.limit=*static_cast<int*>(args[0]);return nullptr;
 case SelectionAnchor:return Box(editor.anchor);
 case SelectionFocus:return Box(editor.cursor);
 case SetAnchor:editor.anchor=*static_cast<int*>(args[0]);return nullptr;
 case SetFocus:editor.cursor=*static_cast<int*>(args[0]);return nullptr;
 case Keyboard:return keyboard;
 case CanGetSelection:case CanSetSelection:return Box(selection_supported);
 case KeyboardSelection:{static NativeRange r;r={std::min(editor.cursor,editor.anchor),std::abs(editor.cursor-editor.anchor)};return reinterpret_cast<Object*>(&r);}
 case SetKeyboardSelection:{auto r=*static_cast<NativeRange*>(args[0]);editor.anchor=r.start;editor.cursor=r.start+r.length;return nullptr;}
 case ScreenHeight:return Box(800);
 case KeyboardArea:{static float area[]={0,0,1280,300};return reinterpret_cast<Object*>(area);}
 case FindChat:assert(*static_cast<int*>(args[1])==0&&*static_cast<int*>(args[2])==0);return array;
 case ArrayLength:return Box(has_chat?(duplicate_chat?2:1):0);
 case ArrayItem:return chat;
 case Active:return Box(true);
 case ChatSend:
  if(chat_exception){*error=chat;return nullptr;}
  ++chat_sends;sent=editor.text;editor.text=editor.ime=new_draft?"newer Android draft":"";
  editor.cursor=editor.anchor=static_cast<int>(editor.text.size());
  if(chat_blur){editor.focused=false;Deselect(Self(),nullptr,nullptr);}
  if(chat_disable)Disable(Self(),nullptr);
  return nullptr;
 }
 return nullptr;
}
Handle Root(Object* self,bool){auto h=++next_root;roots[h]=self;return h;}
Object* TargetRoot(Handle h){return roots.at(h);}
void FreeRoot(Handle h){roots.erase(h);}
const Type* ClassType(Class*){return reinterpret_cast<const Type*>(1);}
Object* TypeObject(const Type*){return reflection;}
void FieldValue(Object* self,Field* field,void* out){assert(self==chat);if(field==m.chatInput)*static_cast<Object**>(out)=matching_field?Self():nullptr;else *static_cast<bool*>(out)=!cooldown;}
void WriteChatFlag(Object* owner,Field* field,void* value){
 assert(owner==chat&&field==m.chatAllowed);cooldown=!*static_cast<bool*>(value);
}
void OriginalChatSend(Object* owner,const Method*){Object* error=nullptr;FakeInvoke(m.chatSend,owner,nullptr,&error);}
void NoEvent(Object*,Object*,const Method*){}
void NoUnary(Object*,const Method*){}
void* Unbox(Object* self){return self;}
void Reset(){
 assert(roots.empty()&&!g_target);editor=Editor{};visible=rewrite=blur_on_change=fail_limit=fail_set=false;
 render_ready=selection_supported=has_chat=matching_field=true;
 cooldown=chat_blur=chat_disable=new_draft=chat_exception=duplicate_chat=false;
 changes=notifications=submits=chat_sends=0;sent.clear();notified.clear();opened_limit=-1;
}
void NativeType(const std::string& text,int cursor=-1,int anchor=-1){
 editor.ime=text;editor.pending_native=true;
 editor.cursor=cursor<0?static_cast<int>(test_utf::FromUtf8(text).size()):cursor;
 editor.anchor=anchor<0?editor.cursor:anchor;
}
void Queue(const char* text,bool all=false,bool quick=false){
 assert(native_shortcuts::Queue(g_shortcuts_session,{all?native_shortcuts::Kind::ReplaceAll:native_shortcuts::Kind::Insert,text,quick}));
}
void Tick(){LateUpdate(Self(),nullptr);}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
extern "C" int DobbyHook(void*,void*,void**){return -1;}
extern "C" int DobbyHookNear(void*,void*,void**){return -1;}
extern "C" EGLContext eglGetCurrentContext(){return nullptr;}
extern "C" EGLSurface eglGetCurrentSurface(EGLint){return nullptr;}
extern "C" EGLBoolean eglQueryContext(EGLDisplay,EGLContext,EGLint,EGLint*){return 0;}
extern "C" EGLBoolean eglQuerySurface(EGLDisplay,EGLSurface,EGLint,EGLint*){return 0;}
extern "C" EGLBoolean eglSwapBuffers(EGLDisplay,EGLSurface){return 1;}
namespace chat_bot_http {
void SetVM(JavaVM*) {}
bool Speak(const std::string&,std::string&) { return false; }
bool Post(const std::string&,const std::string&,const std::string&,int& code,std::string&) { code=0; return false; }
}
namespace keyboard_bridge {
void OpenNative(const char*,bool) {}
bool PollNative(char*,size_t,bool&,bool&) { return false; }
void CloseNative() {}
void SurfaceFrame(int,int){}
bool RenderReady(){return render_ready;}
void Open(const char*,bool){visible=true;}
void Close(){visible=false;}
void ReconcileText(const char*,const char*){}
bool ConsumeEmojiRequest(){return false;}
void SetEmojiSupport(uint64_t,bool){}
void Snapshot(char*,size_t,bool&,bool&,bool&){assert(false&&"Native typing must not poll ImGui text snapshots");}
bool Pointer(float,float,bool){return false;}
}
int main(){
 g_api.runtime_invoke=FakeInvoke;g_api.object_unbox=Unbox;g_api.string_new=NewString;g_api.string_length=Length;g_api.string_chars=Chars;
 g_api.gchandle_new=Root;g_api.gchandle_get_target=TargetRoot;g_api.gchandle_free=FreeRoot;
 g_api.class_get_type=ClassType;g_api.type_get_object=TypeObject;g_api.field_get_value=FieldValue;
 m.text=M(Text);m.setText=M(SetText);m.hide=M(Hide);m.setHide=M(SetHideTag);m.focused=M(Focused);m.readOnly=M(ReadOnly);m.interactable=M(Interactable);
 m.inputType=M(InputType);m.deactivate=M(Deactivate);m.submit=M(SubmitTag);m.alive=M(AliveTag);m.limit=M(Limit);m.setLimit=M(SetLimitTag);
 m.selectionAnchor=M(SelectionAnchor);m.selectionFocus=M(SelectionFocus);m.setSelectionAnchor=M(SetAnchor);m.setSelectionFocus=M(SetFocus);
 m.keyboard=M(Keyboard);m.canGetSelection=M(CanGetSelection);m.canSetSelection=M(CanSetSelection);m.keyboardSelection=M(KeyboardSelection);m.setKeyboardSelection=M(SetKeyboardSelection);
 m.screenH=M(ScreenHeight);m.keyboardArea=M(KeyboardArea);
 m.chatClass=reinterpret_cast<Class*>(1);m.chatInput=reinterpret_cast<Field*>(1);m.chatAllowed=reinterpret_cast<Field*>(2);
 m.findChat=M(FindChat);m.arrayLength=M(ArrayLength);m.arrayItem=M(ArrayItem);m.active=M(Active);m.chatSend=M(ChatSend);
 g_activate=OriginalActivate;g_deselect=NoEvent;g_disable=NoUnary;g_late=OriginalLate;g_changed=OriginalChanged;g_submit_callback=OriginalSubmit;g_ready=true;g_input_ok=true;
 Reset();Activate(Self(),nullptr);
 assert(g_editing&&visible&&editor.limit==0&&!editor.hidden&&editor.native_open&&opened_limit==0);
 // Native edits and autocorrection remain authoritative. Ordinary typing never
 // calls the managed setter again or reopens the IME.
 NativeType("fast Android typing");Tick();assert(editor.text=="fast Android typing"&&changes==0&&notifications==1);
 assert(native_shortcuts::Read().text==editor.text&&native_shortcuts::Read().bottom==.625f);
 Tick();assert(changes==0&&notifications==1);
 // Renderer saw the old text, but a concurrent native word must survive.
 Queue("<");NativeType("fast Android typing plus suggestion");Tick();
 assert(editor.text=="fast Android typing plus suggestion<"&&editor.ime==editor.text);
 // Native UTF-16 selection replaces a complete astral scalar; insertion and
 // subsequent queued edits use the updated caret, not stale full-text copies.
 NativeType(u8"a😀ş",3,1);Tick();Queue("<#F00F>");Queue(">");Tick();
 assert(editor.text==u8"a<#F00F>>ş"&&editor.cursor==9&&editor.anchor==9);
 selection_supported=false;NativeType("abc",1);Tick();Queue("<size=12>");Tick();assert(editor.text=="a<size=12>bc");selection_supported=true;
 // Only clamp when needed; callbacks and submit never observe >1024 scalars.
 const auto before=notifications;NativeType(std::string(1025,'x'));Tick();
 assert(editor.text==std::string(1024,'x')&&editor.ime==editor.text&&notifications==before+1);
 Queue("<size=12>");Tick();assert(editor.text.size()==1024&&native_shortcuts::Read().limited);
 std::string emoji;for(int i=0;i<1024;++i)emoji+=u8"😀";
 NativeType(emoji);Tick();assert(editor.text==emoji&&editor.cursor==2048&&editor.limit==0);
 NativeType(emoji+"x");Tick();assert(editor.text==emoji&&editor.cursor==2048);
 editor.text=std::string(1026,'x');Submit(Self(),nullptr);
 // IME Done on the real game chat input now uses ChatManager.SendMessage.
 assert(submits==1&&editor.text==std::string(1024,'x')&&chat_sends==1&&!visible&&!g_editing);
 editor.focused=false;Tick();assert(!g_editing&&!visible&&editor.limit==25&&!editor.hidden&&roots.empty());
 // Programmatic blur AND an actual outside tap must not send. Draft stays.
 Reset();Activate(Self(),nullptr);editor.text="not submitted";Deselect(Self(),nullptr,nullptr);
 assert(chat_sends==0&&roots.empty());
 Reset();Activate(Self(),nullptr);editor.text="outside tap message";
 g_outside_tap_candidate=true;g_last_outside_tap=std::chrono::steady_clock::now();
 Deselect(Self(),nullptr,nullptr);
 assert(chat_sends==0&&editor.text=="outside tap message"&&!visible&&roots.empty());
 Reset();Activate(Self(),nullptr);editor.text="cooldown message";cooldown=true;
 g_outside_tap_candidate=true;g_last_outside_tap=std::chrono::steady_clock::now();
 Deselect(Self(),nullptr,nullptr);
 assert(chat_sends==0&&roots.empty());
 Reset();Activate(Self(),nullptr);editor.input_type=2;editor.text="password input";
 g_outside_tap_candidate=true;g_last_outside_tap=std::chrono::steady_clock::now();
 Deselect(Self(),nullptr,nullptr);
 assert(chat_sends==0&&roots.empty());
 assert(!native_shortcuts::Read().active);
 // The native path remains available even if the GLES renderer/touch bridge is
 // temporarily unavailable; restore original field settings on every end path.
 Reset();render_ready=false;g_input_ok=false;editor.hidden=true;Activate(Self(),nullptr);
 assert(g_editing&&editor.native_open&&!editor.hidden);Deselect(Self(),nullptr,nullptr);
 assert(editor.hidden&&editor.limit==25&&roots.empty());g_input_ok=true;
 Reset();fail_limit=true;Activate(Self(),nullptr);assert(!g_editing&&editor.focused&&editor.limit==25&&!editor.hidden&&roots.empty());
 Reset();Activate(Self(),nullptr);blur_on_change=true;Queue("another");Tick();assert(!g_editing&&editor.limit==25&&roots.empty());
 Reset();Activate(Self(),nullptr);fail_set=true;Queue("another");Tick();assert(!g_editing&&roots.empty());
 // A stale rendered button cannot affect a newly focused field/session.
 Reset();Activate(Self(),nullptr);const auto old_session=g_shortcuts_session;Queue("old command");End();Activate(Self(),nullptr);
 assert(!native_shortcuts::Queue(old_session,{native_shortcuts::Kind::Insert,"stale"}));Tick();assert(editor.text=="old");End();
 for(int scenario=0;scenario<9;++scenario){
  Reset();Activate(Self(),nullptr);Queue(chat_shortcut::kPayload,true,true);
  if(scenario==1)chat_blur=true;
  if(scenario==2)new_draft=true;
  if(scenario==3)has_chat=false;
  if(scenario==4)cooldown=true;
  if(scenario==5)chat_disable=true;
  if(scenario==6)chat_exception=true;
  if(scenario==7)duplicate_chat=true;
  if(scenario==8)matching_field=false;
  Tick();assert(!submits);
  if(scenario==3||scenario==4||scenario==6||scenario==7||scenario==8)assert(!chat_sends&&editor.text==chat_shortcut::kPayload&&visible);
  else {assert(chat_sends==1&&sent==chat_shortcut::kPayload);
   if(scenario==5)assert(!visible&&!g_target&&roots.empty());
   else assert(visible&&editor.focused&&editor.native_open&&editor.text==(scenario==2?"newer Android draft":""));
  }
  const int sends_before=chat_sends;Tick();assert(chat_sends==sends_before);
  if(g_target)End();
  assert(roots.empty());
 }
 Reset();Activate(Self(),nullptr);cooldown=true;Queue(chat_shortcut::kPayload,true,true);Tick();
 assert(chat_sends==0&&editor.text==chat_shortcut::kPayload);
 cooldown=false;Queue(chat_shortcut::kPayload,true,true);Tick();assert(chat_sends==1);End();
 Reset();cooldown=true;editor.text="manual chat";g_chat_send=OriginalChatSend;g_api.field_set_value=WriteChatFlag;
 chat_send_gate::ready=true;SendChat(chat,nullptr);
 assert(chat_sends==1&&sent=="manual chat"&&editor.text=="manual chat"&&cooldown&&roots.empty());
 editor.text="automatic reply";
 {chat_send_gate::AutomaticSend automatic;SendChat(chat,nullptr);}
 assert(chat_sends==2&&sent=="automatic reply"&&editor.text.empty()&&roots.empty());
 editor.text="manual next";new_draft=true;SendChat(chat,nullptr);
 assert(chat_sends==3&&editor.text=="newer Android draft");
 assert(!chat_send_gate::automatic_send);
 assert(chat_send_gate::Available(false));chat_send_gate::ready=false;assert(!chat_send_gate::Available(false));
 g_api.field_set_value=nullptr;
 // Status alias affects only the synchronous chat scope, never the stored nick.
 g_chat_nick=+[](const Method*)->String*{static String name{u"Bot"};return &name;};
 String system_name{u"System"};
 assert(g_api.Utf8(ChatNick(nullptr))=="Bot");
 {
  ScopedRoot name(reinterpret_cast<Object*>(&system_name));
  chat_send_gate::StatusAlias request(name.handle);
  assert(g_api.Utf8(ChatNick(nullptr))=="Bot");
  {
   chat_send_gate::StatusSend send;
   assert(g_api.Utf8(ChatNick(nullptr))=="System");
   {chat_send_gate::StatusSend nested;assert(g_api.Utf8(ChatNick(nullptr))=="Bot");}
   assert(g_api.Utf8(ChatNick(nullptr))=="System");
  }
  assert(g_api.Utf8(ChatNick(nullptr))=="Bot");
 }
 assert(roots.empty()&&!chat_send_gate::status_alias&&!chat_send_gate::active_status_alias);
 std::cout<<"Android editor: native typing/autocorrection, latest-text FIFO shortcuts, UTF-16 caret/selection, 1024-scalar callback/submit cap, restore/reentry/fallback and stale session rejection: PASS\n";
 std::cout<<"Native square shortcut: normal chat send, cooldown/owner checks, native refocus, newer Android draft and scene guards: PASS\n";
}
