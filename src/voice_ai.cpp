#include "voice_ai.h"
#include "voice_ai_core.h"
#include "chat_bot_http.h"
#include "chat_send_gate.h"
#include "dobby.h"
#include <android/log.h>
#include <atomic>
#include <array>
#include <cstddef>
#include <thread>
namespace voice_ai {namespace {
using namespace il2cpp;using namespace voice_ai_core;
struct Root {const Api& api;Handle h=0;Root(const Api& a,Object* o=nullptr):api(a){Set(o);}~Root(){if(h)api.gchandle_free(h);}Root(const Root&)=delete;void Set(Object* o){Handle n=o?api.gchandle_new(o,true):0;if(h)api.gchandle_free(h);h=n;}Object* Get()const{return h?api.gchandle_get_target(h):nullptr;}};
using FrameFn=void(*)(Object*,Object*,const Method*);FrameFn original_frame=nullptr;
using BoolFn=void(*)(Object*,bool,const Method*);using IntFn=void(*)(Object*,int,const Method*);
BoolFn original_record=nullptr,original_transmit=nullptr;IntFn original_source=nullptr;
const Api* mic_api=nullptr;Field *mic_source_field=nullptr,*mic_record_field=nullptr;
using StartFn=void(*)(Object*,const Method*);StartFn original_start=nullptr;
std::atomic<bool> mic_guard_active{false};
struct ExtraMic {Handle handle=0;bool wanted=false;};std::mutex extra_mutex;std::map<Object*,ExtraMic> extra_mics;
bool MicSource(Object* o){if(!mic_api||!mic_source_field)return false;int source=-1;mic_api->field_get_value(o,mic_source_field,&source);return source==0;}
void RememberMic(Object* o,const bool* requested=nullptr){
 if(!mic_api||!mic_record_field)return;
 std::lock_guard<std::mutex> l(extra_mutex);
 auto it=extra_mics.find(o);if(it==extra_mics.end()){if(extra_mics.size()>=128)return;bool current=false;mic_api->field_get_value(o,mic_record_field,&current);it=extra_mics.emplace(o,ExtraMic{mic_api->gchandle_new(o,true),current}).first;}
 if(requested)it->second.wanted=*requested;
}
std::atomic<Object*> leased_recorder{nullptr};std::atomic<bool> wanted_record{false},wanted_transmit{false},normal_record{false},normal_transmit{false};std::atomic<int> wanted_source{0},normal_source{0};thread_local bool internal_set=false;
void RecordHook(Object* o,bool v,const Method* m){if(!internal_set&&o==leased_recorder.load()){normal_record=v;v=wanted_record.load();}else if(!internal_set&&mic_guard_active.load()){bool tracked=false;{std::lock_guard<std::mutex> l(extra_mutex);tracked=extra_mics.count(o);}if(tracked||MicSource(o)){RememberMic(o,&v);v=false;}}original_record(o,v,m);}
void TransmitHook(Object* o,bool v,const Method* m){if(!internal_set&&o==leased_recorder.load()){normal_transmit=v;v=wanted_transmit.load();}original_transmit(o,v,m);}
void SourceHook(Object* o,int v,const Method* m){if(!internal_set&&o==leased_recorder.load()){normal_source=v;v=wanted_source.load();}original_source(o,v,m);}
void StartHook(Object* o,const Method* m){
 if(!internal_set&&mic_guard_active.load()&&(MicSource(o)||(o==leased_recorder.load()&&!wanted_record.load()))){if(o!=leased_recorder.load())RememberMic(o);return;}original_start(o,m);
}
struct Internal {bool old=internal_set;Internal(){internal_set=true;}~Internal(){internal_set=old;}};
struct Stream {Root link;int actor,rate,channels;std::string name;Stream(const Api& a,Object* p,int id,int r,int c,std::string n):link(a,p),actor(id),rate(r),channels(c),name(std::move(n)){} };
const Api* hook_api=nullptr;Field *buf_field=nullptr,*eos_field=nullptr;Class* float_class=nullptr;std::atomic<bool> capture{false};std::atomic<uint64_t> capture_epoch{1};std::mutex segments_mutex;Segmenter segments;
using Streams=std::vector<std::shared_ptr<Stream>>;std::shared_ptr<const Streams> streams=std::make_shared<Streams>();
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
float* ArrayData(const Api& api,Object* array,size_t count){
 if(!array||!api.array_length||!api.array_object_header_size||!api.array_element_size||!api.class_get_element_class||!api.object_get_class)return nullptr;
 auto* c=api.object_get_class(array);if(api.class_get_element_class(c)!=float_class||api.array_element_size(c)!=sizeof(float)||api.array_length(array)!=count)return nullptr;
 const size_t header=api.array_object_header_size();if(header<2*sizeof(void*)||header>128||header%alignof(float))return nullptr;
 return reinterpret_cast<float*>(reinterpret_cast<unsigned char*>(array)+header);
}
void FrameHook(Object* link,Object* frame,const Method* m){
 try{if(capture.load(std::memory_order_acquire)&&frame&&buf_field&&hook_api){
  const auto token=capture_epoch.load();auto specs=std::atomic_load(&streams);for(const auto& spec:*specs)if(spec->link.Get()==link){
   Object* raw=nullptr;bool end=false;hook_api->field_get_value(frame,buf_field,&raw);if(eos_field)hook_api->field_get_value(frame,eos_field,&end);
   if(raw){Root pin(*hook_api,raw);size_t n=hook_api->array_length(pin.Get());if(n&&n<=19200){auto* pcm=ArrayData(*hook_api,pin.Get(),n);if(pcm){std::unique_lock<std::mutex> l(segments_mutex,std::try_to_lock);if(l.owns_lock()&&capture.load()&&token==capture_epoch.load())segments.Push(spec->actor,spec->name,pcm,n,spec->rate,spec->channels,Now(),end);}}}break;
  }
 }}catch(...){/* Audio playback always forwards. No network or Unity API here. */}
 original_frame(link,frame,m);
}
struct Vec3 {float x=0,y=0,z=0;};
struct Runtime {
 const Api& a;std::thread::id unity=std::this_thread::get_id();bool initialized=false,valid=false,hooks=false,lease=false,network_play=false;unsigned revision=0;uint64_t epoch=1;uintptr_t room_id=0;double next_discovery=0,next_request=0,playing_until=0,text_deadline=0,text_next=0;
 voice_ai_bridge::Snapshot snap;voice_ai_bridge::Settings settings;voice_ai_bridge::Config cfg;
 Root room_root,manager,recorder,lease_owner,old_clip,clip,local_go,local_source;bool old_loop=false,old_detection=false;
 Worker worker;std::deque<Request> requests;std::deque<std::string> text_out;std::vector<chat_bot_core::Turn> history;
 Class *manager_class{},*view_class{},*rec_class{},*source_class{};
 const Method *find{},*length{},*item{},*alive{},*go{},*active{},*current_room{},*manager_get{},*available{},*speaker_get{},*remote_get{},*owner_get{},*mine{},*actor_get{},*nick{},*rate_get{},*channels_get{},
 *get_record{},*set_record{},*get_transmit{},*set_transmit{},*get_source{},*set_source{},*get_clip{},*set_clip{},*get_loop{},*set_loop{},*get_detect{},*set_detect{},
 *clip_create{},*clip_data{},*destroy{},*go_ctor{},*add_component{},*audio_clip{},*audio_loop{},*audio_spatial{},*audio_play{},*audio_stop{},
 *chat_send{},*input_text{},*input_set{},*input_focus{},*room_properties{},*hash_item{},*voice_client{},*voice_joined{};
 Class* chat_class{};Field *can_send{},*voice_info{};
 const Method* M(const char* ns,const char* cls,const char* name,int n=0){return a.FindMethod(a.Find(ns,cls),name,n);}
 Object* Ref(Object* o,const char* n){if(!o)return nullptr;auto* f=a.class_get_field_from_name(a.object_get_class(o),n);Object* v=nullptr;if(f){auto* t=a.class_from_type(a.field_get_type(f));const char* expected=!std::strcmp(n,"recorder")?"Recorder":!std::strcmp(n,"photonView")?"PhotonView":!std::strcmp(n,"inputField")?"TMP_InputField":!std::strcmp(n,"punVoiceClient")?"PunVoiceClient":"";if(t&&!std::strcmp(a.class_get_name(t),expected))a.field_get_value(o,f,&v);}return v;}
 bool Alive(Object* o){if(!o)return false;void* args[]={o};bool ok=false;Root result(a,a.Call(alive,nullptr,args,&ok));return ok&&result.Get()&&*static_cast<bool*>(a.object_unbox(result.Get()));}
 bool Active(Object* o){if(!Alive(o))return false;Root g(a,a.Call(go,o));return Alive(g.Get())&&a.Value<bool>(active,g.Get(),false);}
 template<class T>bool Set(const Method* m,Object* o,T v){void* args[]={&v};bool ok=false;Internal guard;a.Call(m,o,args,&ok);return ok;}
 bool SetRef(const Method* m,Object* o,Object* v){void* args[]={v};bool ok=false;Internal guard;a.Call(m,o,args,&ok);return ok;}
 template<class T>bool Read(const Method* m,Object* o,T& v){bool ok=false;Root result(a,a.Call(m,o,nullptr,&ok));if(!ok||!result.Get())return false;v=*static_cast<T*>(a.object_unbox(result.Get()));return true;}
 template<class F>bool Each(Class* c,F f){if(!c)return false;Root type(a,a.type_get_object(a.class_get_type(c)));int inactive=1,sort=0;void* args[]={type.Get(),&inactive,&sort};bool ok=false;Root array(a,a.Call(find,nullptr,args,&ok));if(!ok||!array.Get())return false;int n=a.Value<int>(length,array.Get(),-1);if(n<0||n>128)return false;for(int i=0;i<n;++i){void* at[]={&i};Root o(a,a.Call(item,array.Get(),at,&ok));if(!ok)return false;if(Alive(o.Get()))f(o.Get());}return true;}
 bool Allowed(const std::string& name)const{return cfg.all||std::find(cfg.selected.begin(),cfg.selected.end(),name)!=cfg.selected.end();}
 bool Resolve(){
  if(!a.field_get_value||!a.class_from_type||!a.field_get_type||!a.array_length||!a.array_object_header_size||!a.array_element_size||!a.class_get_element_class||!a.method_get_param||!a.class_value_size||!a.array_new||!a.object_new)return false;
  float_class=a.Find("System","Single");manager_class=a.Find("","VoiceChatManager");view_class=a.Find("Photon.Voice.PUN","PhotonVoiceView");rec_class=a.Find("Photon.Voice.Unity","Recorder");source_class=a.Find("UnityEngine","AudioSource");
  find=M("UnityEngine","Object","FindObjectsByType",3);length=M("System","Array","get_Length");item=M("System","Array","GetValueImpl",1);alive=M("UnityEngine","Object","op_Implicit",1);go=M("UnityEngine","Component","get_gameObject");active=M("UnityEngine","GameObject","get_activeInHierarchy");current_room=M("Photon.Pun","PhotonNetwork","get_CurrentRoom");
  manager_get=a.FindMethod(manager_class,"gpy",0);available=a.FindMethod(manager_class,"gqc",0);speaker_get=a.FindMethod(view_class,"get_SpeakerInUse",0);remote_get=M("Photon.Voice.Unity","Speaker","get_RemoteVoice");owner_get=M("Photon.Pun","PhotonView","get_Owner");mine=M("Photon.Pun","PhotonView","get_IsMine");actor_get=M("Photon.Realtime","Player","get_ActorNumber");nick=M("Photon.Realtime","Player","get_NickName");rate_get=M("Photon.Voice","VoiceInfo","get_SamplingRate");channels_get=M("Photon.Voice","VoiceInfo","get_Channels");
  auto* link_class=a.Find("Photon.Voice.Unity","RemoteVoiceLink");voice_info=link_class?a.class_get_field_from_name(link_class,"VoiceInfo"):nullptr;
  mic_api=&a;mic_source_field=rec_class?a.class_get_field_from_name(rec_class,"sourceType"):nullptr;mic_record_field=rec_class?a.class_get_field_from_name(rec_class,"recordingEnabled"):nullptr;
  if(!mic_source_field||!mic_record_field)return false;
  auto* source_type=a.class_from_type(a.field_get_type(mic_source_field));uint32_t source_align=0;auto* record_type=a.class_from_type(a.field_get_type(mic_record_field));
  if(!source_type||a.class_value_size(source_type,&source_align)!=4||!record_type||std::strcmp(a.class_get_name(record_type),"Boolean")||std::strcmp(a.class_get_namespace(record_type),"System"))return false;
  const auto prop=[&](const char* n){return a.FindMethod(rec_class,n,0);};const auto setter=[&](const char* n){return a.FindMethod(rec_class,n,1);};
  get_record=prop("get_RecordingEnabled");set_record=setter("set_RecordingEnabled");get_transmit=prop("get_TransmitEnabled");set_transmit=setter("set_TransmitEnabled");get_source=prop("get_SourceType");set_source=setter("set_SourceType");get_clip=prop("get_AudioClip");set_clip=setter("set_AudioClip");get_loop=prop("get_LoopAudioClip");set_loop=setter("set_LoopAudioClip");get_detect=prop("get_VoiceDetection");set_detect=setter("set_VoiceDetection");
  clip_create=M("UnityEngine","AudioClip","Create",6);clip_data=M("UnityEngine","AudioClip","SetData",2);destroy=M("UnityEngine","Object","Destroy",1);go_ctor=M("UnityEngine","GameObject",".ctor",0);add_component=M("UnityEngine","GameObject","AddComponent",1);audio_clip=M("UnityEngine","AudioSource","set_clip",1);audio_loop=M("UnityEngine","AudioSource","set_loop",1);audio_spatial=M("UnityEngine","AudioSource","set_spatialBlend",1);audio_play=M("UnityEngine","AudioSource","Play",0);audio_stop=M("UnityEngine","AudioSource","Stop",0);
  chat_class=a.Find("","ChatManager");chat_send=a.FindMethod(chat_class,"SendMessage",0);can_send=chat_class?a.class_get_field_from_name(chat_class,"canSendMessage"):nullptr;input_text=M("TMPro","TMP_InputField","get_text");input_set=M("TMPro","TMP_InputField","set_text",1);input_focus=M("TMPro","TMP_InputField","get_isFocused");
  if(!find||!length||!item||!alive||!go||!active||!current_room||!manager_get||!available||!voice_info||!speaker_get||!remote_get||!owner_get||!mine||!actor_get||!nick||!rate_get||!channels_get||!float_class||!clip_create||!clip_data||!destroy||!go_ctor||!add_component||!audio_clip||!audio_loop||!audio_spatial||!audio_play||!audio_stop)return false;
  for(auto* p:{get_record,set_record,get_transmit,set_transmit,get_source,set_source,get_clip,set_clip,get_loop,set_loop,get_detect,set_detect})if(!p)return false;
  room_properties=M("Photon.Realtime","RoomInfo","get_CustomProperties");voice_client=M("Photon.Voice.Unity","VoiceConnection","get_Client");voice_joined=M("Photon.Realtime","LoadBalancingClient","get_InRoom");
  if(a.class_get_methods&&a.method_get_param_count){auto* table=a.Find("ExitGames.Client.Photon","Hashtable");void* iter=nullptr;for(int i=0;table&&i<256;++i){auto* method=a.class_get_methods(table,&iter);if(!method)break;if(std::strcmp(a.method_get_name(method),"get_Item")||a.method_get_param_count(method)!=1)continue;auto* p=a.class_from_type(a.method_get_param(method,0));if(p&&!std::strcmp(a.class_get_namespace(p),"System")&&!std::strcmp(a.class_get_name(p),"Object")){hash_item=method;break;}}}
  // Real instantiated FrameOut<float>, not a guessed generic structure.
  auto* method=a.FindMethod(link_class,"OnDecodedFrameFloatAction",1);auto* frame=method?a.class_from_type(a.method_get_param(method,0)):nullptr;
  buf_field=frame?a.class_get_field_from_name(frame,"<Buf>k__BackingField"):nullptr;eos_field=frame?a.class_get_field_from_name(frame,"<EndOfStream>k__BackingField"):nullptr;
  if(!method||!buf_field||!eos_field)return false;
  auto* array_type=a.class_from_type(a.field_get_type(buf_field));auto* eos_type=a.class_from_type(a.field_get_type(eos_field));
  if(!array_type||a.class_get_element_class(array_type)!=float_class||!eos_type||std::strcmp(a.class_get_namespace(eos_type),"System")||std::strcmp(a.class_get_name(eos_type),"Boolean"))return false;
  const auto hook=[&](const Method* m,void* replacement,auto& original){void* target=Entry(m);return target&&DobbyHook(target,replacement,reinterpret_cast<void**>(&original))==0&&original;};
  hook_api=&a;bool f=hook(method,reinterpret_cast<void*>(FrameHook),original_frame);
  bool r=hook(set_record,reinterpret_cast<void*>(RecordHook),original_record),t=hook(set_transmit,reinterpret_cast<void*>(TransmitHook),original_transmit),s=hook(set_source,reinterpret_cast<void*>(SourceHook),original_source);bool start_hook=hook(a.FindMethod(rec_class,"StartRecording",0),reinterpret_cast<void*>(StartHook),original_start);hooks=r&&t&&s&&start_hook;
  __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG voice hooks decoded=%d mic_guards=%d",f,hooks);return f;
 }
 void Reset(bool disable){
  worker.Cancel();capture=false;++capture_epoch;std::atomic_store(&streams,std::shared_ptr<const Streams>(std::make_shared<Streams>()));{std::lock_guard<std::mutex> l(segments_mutex);segments.Clear();}
  StopAudio();++epoch;requests.clear();text_out.clear();history.clear();next_request=Now()+.6;snap.heard.clear();snap.reply.clear();if(disable){settings.enabled=false;std::lock_guard<std::mutex> l(voice_ai_bridge::mutex);voice_ai_bridge::desired.enabled=false;voice_ai_bridge::texts.clear();}
 }
 bool NetworkAllowed(){
  if(!room_id||!room_properties||!hash_item||!voice_client||!voice_joined||!Active(manager.Get())||!a.Value<bool>(available,manager.Get(),false)||!Alive(recorder.Get()))return false;
  Root room(a,a.Call(current_room));if(reinterpret_cast<uintptr_t>(room.Get())!=room_id)return false;
  Root props(a,a.Call(room_properties,room.Get())),key(a,reinterpret_cast<Object*>(a.string_new("VoiceChatEnabled")));if(!props.Get()||!key.Get())return false;
  void* args[]={key.Get()};Root allowed(a,a.Call(hash_item,props.Get(),args));if(!allowed.Get())return false;auto* type=a.object_get_class(allowed.Get());if(std::strcmp(a.class_get_namespace(type),"System")||std::strcmp(a.class_get_name(type),"Boolean")||!*static_cast<bool*>(a.object_unbox(allowed.Get())))return false;
  Root connection(a,Ref(manager.Get(),"punVoiceClient")),client(a,connection.Get()?a.Call(voice_client,connection.Get()):nullptr);return client.Get()&&a.Value<bool>(voice_joined,client.Get(),false);
 }
 void Discover(){
  manager.Set(a.Call(manager_get));recorder.Set(Active(manager.Get())?Ref(manager.Get(),"recorder"):nullptr);snap.network_ready=NetworkAllowed();
  auto found=std::make_shared<Streams>();
  if(settings.enabled&&settings.input!=1&&snap.network_ready){Each(view_class,[&](Object* view){
   if(found->size()>=8||!Active(view))return;
   Root pv(a,Ref(view,"photonView"));if(!Alive(pv.Get())||a.Value<bool>(mine,pv.Get(),true))return;
   Root owner(a,a.Call(owner_get,pv.Get())),name(a,a.Call(nick,owner.Get()));int actor=a.Value<int>(actor_get,owner.Get(),0);std::string label=chat_bot_core::StripTags(a.Utf8(reinterpret_cast<String*>(name.Get())));if(actor<=0||label.empty()||!Allowed(label)||std::any_of(found->begin(),found->end(),[&](const auto& old){return old->actor==actor;}))return;
   Root speaker(a,a.Call(speaker_get,view)),link(a,speaker.Get()?a.Call(remote_get,speaker.Get()):nullptr);if(!Alive(speaker.Get())||!link.Get())return;
   auto* info_type=a.class_from_type(a.field_get_type(voice_info));uint32_t align=0;int bytes=a.class_value_size(info_type,&align);if(bytes<=0||bytes>256||align>alignof(std::max_align_t))return;
   alignas(std::max_align_t) std::array<unsigned char,256> info{};a.field_get_value(link.Get(),voice_info,info.data());int rate=0,channels=0;
   if(!Read(rate_get,reinterpret_cast<Object*>(info.data()),rate)||!Read(channels_get,reinterpret_cast<Object*>(info.data()),channels)||rate<8000||rate>48000||channels<1||channels>2)return;
   found->push_back(std::make_shared<Stream>(a,link.Get(),actor,rate,channels,label));
  });}
  snap.streams=unsigned(found->size());std::atomic_store(&streams,std::shared_ptr<const Streams>(found));
 }
 bool AcquireMic(bool network,bool defer_transmit=false){
  if(!Alive(recorder.Get()))return !snap.network_ready; // no local mic stream exists
  if(!hooks)return false;
  bool record=false,transmit=false;int source=0;if(!Read(get_record,recorder.Get(),record)||!Read(get_transmit,recorder.Get(),transmit)||!Read(get_source,recorder.Get(),source)||source<0||source>2||!Read(get_loop,recorder.Get(),old_loop)||!Read(get_detect,recorder.Get(),old_detection))return false;
  lease_owner.Set(recorder.Get());old_clip.Set(a.Call(get_clip,recorder.Get()));normal_record=record;normal_transmit=transmit;normal_source=source;wanted_record=false;wanted_transmit=false;wanted_source=source;leased_recorder=lease_owner.Get();lease=true;
  if(!Set(set_transmit,lease_owner.Get(),false)||!Set(set_record,lease_owner.Get(),false))return false;
  bool readback=true;if(!Read(get_record,lease_owner.Get(),readback)||readback)return false;
  if(network){wanted_source=1;if(!Set(set_source,lease_owner.Get(),1)||!SetRef(set_clip,lease_owner.Get(),clip.Get())||!Set(set_loop,lease_owner.Get(),false)||!Set(set_detect,lease_owner.Get(),false))return false;
   int check=-1;if(!Read(get_source,lease_owner.Get(),check)||check!=1||a.Call(get_clip,lease_owner.Get())!=clip.Get())return false;
   wanted_record=true;if(!Set(set_record,lease_owner.Get(),true))return false;
   if(!Read(get_record,lease_owner.Get(),readback)||!readback)return false;
   if(!defer_transmit){wanted_transmit=true;if(!Set(set_transmit,lease_owner.Get(),true))return false;}
  }snap.mic_blocked=true;return true;
 }
 bool MuteExtraMics(){
  mic_guard_active=true;bool ok=true;
  if(!Each(rec_class,[&](Object* o){if(o==lease_owner.Get()||!Active(o)||!MicSource(o))return;RememberMic(o);bool value=true;if(!Set(set_record,o,false)||!Read(get_record,o,value)||value)ok=false;}))return false;
  return ok;
 }
 void StopAudio(){
  capture=false;
  if(Alive(local_source.Get()))a.Call(audio_stop,local_source.Get());
  if(lease&&Alive(lease_owner.Get())){
   Set(set_transmit,lease_owner.Get(),false);Set(set_record,lease_owner.Get(),false);
   bool same=room_id&&reinterpret_cast<uintptr_t>(a.Call(current_room))==room_id&&Active(manager.Get())&&Ref(manager.Get(),"recorder")==lease_owner.Get();
   if(same){SetRef(set_clip,lease_owner.Get(),old_clip.Get());Set(set_loop,lease_owner.Get(),old_loop);Set(set_detect,lease_owner.Get(),old_detection);Set(set_source,lease_owner.Get(),normal_source.load());Set(set_record,lease_owner.Get(),normal_record.load());Set(set_transmit,lease_owner.Get(),normal_transmit.load());}
  }
  mic_guard_active=false;
  std::map<Object*,ExtraMic> released;{std::lock_guard<std::mutex> l(extra_mutex);released.swap(extra_mics);}
  bool same_room=room_id&&reinterpret_cast<uintptr_t>(a.Call(current_room))==room_id;
  for(auto& [pointer,mic]:released){auto* o=a.gchandle_get_target(mic.handle);if(same_room&&o!=lease_owner.Get()&&Active(o)){Set(set_record,o,false);Set(set_record,o,mic.wanted);}a.gchandle_free(mic.handle);}
  leased_recorder=nullptr;lease=false;network_play=false;lease_owner.Set(nullptr);old_clip.Set(nullptr);
  for(auto* root:{&local_go,&clip}){if(Alive(root->Get())){void* args[]={root->Get()};a.Call(destroy,nullptr,args);}root->Set(nullptr);}local_source.Set(nullptr);snap.speaking=snap.mic_blocked=false;playing_until=0;
 }
 bool Play(const Audio& audio,double now){
  if(audio.samples.empty()||audio.rate<8000||audio.rate>48000||audio.samples.size()>size_t(audio.rate)*45)return false;
  StopAudio();std::vector<float> padded(size_t(audio.rate)*.35f,0);padded.insert(padded.end(),audio.samples.begin(),audio.samples.end());padded.resize(padded.size()+size_t(audio.rate)/4);
  Root samples(a,a.array_new(float_class,padded.size()));auto* ptr=ArrayData(a,samples.Get(),padded.size());if(!ptr)return false;std::memcpy(ptr,padded.data(),padded.size()*sizeof(float));
  Root name(a,reinterpret_cast<Object*>(a.string_new("GMM AI voice")));int n=int(padded.size()),channels=1,rate=audio.rate;bool stream=false,ok=false;void* args[]={name.Get(),&n,&channels,&rate,&stream,nullptr};clip.Set(a.Call(clip_create,nullptr,args,&ok));if(!ok||!Alive(clip.Get()))return false;
  int offset=0;void* put[]={samples.Get(),&offset};Root written(a,a.Call(clip_data,clip.Get(),put,&ok));if(!ok||!written.Get()||!*static_cast<bool*>(a.object_unbox(written.Get())))return false;
  snap.network_ready=NetworkAllowed();network_play=snap.network_ready&&settings.destination==0;
  if(!AcquireMic(network_play,true)||!MuteExtraMics()){StopAudio();return false;}
  {
   local_go.Set(a.object_new(a.Find("UnityEngine","GameObject")));a.Call(go_ctor,local_go.Get(),nullptr,&ok);if(!ok||!Alive(local_go.Get())){StopAudio();return false;}
   Root type(a,a.type_get_object(a.class_get_type(source_class)));void* add[]={type.Get()};local_source.Set(a.Call(add_component,local_go.Get(),add,&ok));
   if(!ok||!Alive(local_source.Get())||!SetRef(audio_clip,local_source.Get(),clip.Get())||!Set(audio_loop,local_source.Get(),false)||!Set(audio_spatial,local_source.Get(),0.f)){StopAudio();return false;}
   if(network_play){
    wanted_record=false;if(!Set(set_record,lease_owner.Get(),false)){StopAudio();return false;}
    wanted_record=true;if(!Set(set_record,lease_owner.Get(),true)){StopAudio();return false;}
    wanted_transmit=true;if(!Set(set_transmit,lease_owner.Get(),true)){StopAudio();return false;}
   }
   a.Call(audio_play,local_source.Get(),nullptr,&ok);if(!ok){StopAudio();return false;}
  }
  snap.speaking=true;playing_until=now+double(n)/rate+.6;snap.status=network_play?"AI ses kanalında konuşuyor; mikrofon devre dışı.":"AI bu cihazda konuşuyor; oda ses kanalı kullanılmıyor.";
  __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG voice play network=%d mic_blocked=%d samples=%d rate=%d",network_play,snap.mic_blocked,n,rate);return true;
 }
 bool SendText(const std::string& line){
  if(!can_send||!chat_send||!input_text||!input_set||!input_focus)return false;
  Root chat(a);bool ambiguous=false;Each(chat_class,[&](Object* o){if(!Active(o))return;if(chat.Get()&&chat.Get()!=o)ambiguous=true;chat.Set(o);});if(!chat.Get()||ambiguous)return false;
  bool allowed=false;a.field_get_value(chat.Get(),can_send,&allowed);if(!chat_send_gate::Available(allowed))return false;Root input(a,Ref(chat.Get(),"inputField"));if(!Alive(input.Get())||a.Value<bool>(input_focus,input.Get(),true))return false;
  Root old(a,a.Call(input_text,input.Get()));if(!old.Get()||!a.Utf8(reinterpret_cast<String*>(old.Get())).empty())return false;
  Root text(a,reinterpret_cast<Object*>(a.string_new(line.c_str())));void* args[]={text.Get()};bool ok=false;a.Call(input_set,input.Get(),args,&ok);if(!ok)return false;a.Call(chat_send,chat.Get(),nullptr,&ok);bool sent=ok;Root after(a,a.Call(input_text,input.Get()));if(!after.Get())return false;
  std::string current=a.Utf8(reinterpret_cast<String*>(after.Get()));if(current==line){void* restore[]={old.Get()};a.Call(input_set,input.Get(),restore);return false;}return sent&&current.empty()&&reinterpret_cast<uintptr_t>(a.Call(current_room))==room_id;
 }
 void Queue(Request q){if(!Allowed(q.name)||cfg.key.empty())return;q.epoch=epoch;q.room=room_id;q.key=cfg.key;q.system=cfg.system;q.own_name=cfg.own_name;q.strict=cfg.all&&cfg.strict;q.voice=settings.output==1;q.history=history;if(!requests.empty()&&Merge(requests.back(),q))return;if(requests.size()>=3)requests.pop_front();requests.push_back(std::move(q));}
public:
 void Abort(){Reset(true);valid=false;}
 explicit Runtime(const Api& api):a(api),room_root(a),manager(a),recorder(a),lease_owner(a),old_clip(a),clip(a),local_go(a),local_source(a){}
 void Tick(bool paused){
  if(std::this_thread::get_id()!=unity)return;
  double now=Now();if(!initialized){initialized=true;valid=Resolve();snap.ready=valid;}
  if(!valid){snap.status="Sesli AI: bu sürümün ses bağlantıları eksik.";voice_ai_bridge::Publish(snap);return;}
  voice_ai_bridge::Settings wanted;voice_ai_bridge::Config next_cfg;std::deque<voice_ai_bridge::Text> texts;unsigned next_rev;bool cancel;
  {std::lock_guard<std::mutex> l(voice_ai_bridge::mutex);wanted=voice_ai_bridge::desired;next_cfg=voice_ai_bridge::config;next_rev=voice_ai_bridge::revision;cancel=voice_ai_bridge::cancel;voice_ai_bridge::cancel=false;texts.swap(voice_ai_bridge::texts);}
  Root observed_room(a,paused?nullptr:a.Call(current_room));uintptr_t next_room=reinterpret_cast<uintptr_t>(observed_room.Get());
  if(next_room!=room_id){Reset(room_id!=0);room_root.Set(observed_room.Get());room_id=next_room;wanted.enabled=false;{std::lock_guard<std::mutex> l(voice_ai_bridge::mutex);voice_ai_bridge::desired.enabled=false;}next_discovery=0;}
  bool config_changed=next_cfg.key!=cfg.key||next_cfg.system!=cfg.system||next_cfg.own_name!=cfg.own_name||next_cfg.all!=cfg.all||next_cfg.strict!=cfg.strict||next_cfg.selected!=cfg.selected;
  if(next_rev!=revision||config_changed||cancel){Reset(false);revision=next_rev;settings=wanted;cfg=std::move(next_cfg);texts.clear();next_discovery=0;snap.status=settings.enabled?"Sesli AI hazır; yeni ses veya mesaj bekleniyor.":"Sesli AI kapalı.";}
  if(settings.enabled&&(cfg.key.empty()||(!cfg.all&&cfg.selected.empty())||!room_id)){settings.enabled=false;std::lock_guard<std::mutex> l(voice_ai_bridge::mutex);voice_ai_bridge::desired.enabled=false;snap.status=cfg.key.empty()?"Önce AI bölümünden API anahtarı gir.":!room_id?"Bir odaya gir.":"AI > Kişiler bölümünden kişi seç veya Herkes'i aç.";}
  if(now>=next_discovery){next_discovery=now+1;Discover();}
  if(snap.speaking){
   bool invalid=now>=playing_until||!settings.enabled;
   if(network_play){int source=-1;invalid=invalid||!NetworkAllowed()||!Alive(lease_owner.Get())||!Read(get_source,lease_owner.Get(),source)||source!=1||a.Call(get_clip,lease_owner.Get())!=clip.Get();}
   if(invalid){StopAudio();next_request=now+.6;}
  }
  capture=valid&&settings.enabled&&settings.input!=1&&!snap.speaking&&now>=next_request&&room_id;
  {std::lock_guard<std::mutex> l(segments_mutex);segments.Flush(now);Utterance utterance;while(segments.Take(utterance)){Request q;q.actor=utterance.actor;q.name=utterance.name;q.pcm=std::move(utterance.samples);Queue(std::move(q));}}
  if(settings.enabled&&settings.input!=0)for(auto& t:texts)if(t.room==room_id&&t.name!=cfg.own_name){Request q;q.name=std::move(t.name);q.text=std::move(t.text);Queue(std::move(q));}
  Result result;if(worker.Poll(result)&&result.epoch==epoch&&result.room==room_id&&settings.enabled){
   __android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM_DIAG voice result actor=%d http=%d skipped=%d audio_samples=%zu",result.actor,result.code,result.skipped,result.audio.samples.size());
   snap.heard=chat_bot_core::PrefixScalars(result.heard,120);snap.reply=chat_bot_core::PrefixScalars(result.reply,160);snap.status=result.status;
   if(!result.reply.empty()){
    history.push_back({false,result.name+": "+result.heard});history.push_back({true,result.reply});chat_bot_core::TrimHistory(history,12);
    if(result.audio.samples.empty()||!Play(result.audio,now)){for(auto& part:chat_bot_core::SplitChunks(result.reply,200,8))text_out.push_back(part);text_deadline=now+30;text_next=now;}
   }next_request=now+2;
  }
  if(!text_out.empty()&&now>text_deadline)text_out.clear();
  if(settings.enabled&&!text_out.empty()&&now>=text_next){text_next=now+1.2;if(SendText(text_out.front()))text_out.pop_front();}
  if(settings.enabled&&!worker.Busy()&&!snap.speaking&&text_out.empty()&&!requests.empty()&&now>=next_request){Request q=std::move(requests.front());requests.pop_front();if(q.epoch==epoch){q.history=history;worker.Start(&chat_bot_http::Post,std::move(q));snap.status="Sesli AI işliyor...";}}
  snap.settings=settings;snap.busy=worker.Busy();if(!settings.enabled&&!snap.speaking&&snap.status.empty())snap.status="Sesli AI kapalı.";voice_ai_bridge::Publish(snap);
 }
};Runtime* runtime=nullptr;
}
void Pump(const Api& api,bool paused){try{if(!runtime){voice_ai_core::tts=&chat_bot_http::Speak;runtime=new Runtime(api);}runtime->Tick(paused);}catch(...){capture=false;if(runtime){try{runtime->Abort();}catch(...){}}voice_ai_bridge::Snapshot s;s.status="Sesli AI hata nedeniyle durdu.";voice_ai_bridge::Publish(s);}}
}
