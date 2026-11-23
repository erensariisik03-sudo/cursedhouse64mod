#include "../src/voice_ai.cpp"
#include <cassert>
#include <iostream>
#include <map>
using namespace il2cpp;
namespace {
struct C{const char* ns;const char* name;C* element=nullptr;};struct F{C* type;};
struct N{C* c;N* go=nullptr;N* recorder=nullptr;bool alive=true,record=true,transmit=true,loop=true,detection=true;int source=0;N* clip=nullptr;};
struct Buffer{uint64_t placeholder[5]{};float data[6400]{};};Buffer buffer;
C single{"System","Single"},boolean{"System","Boolean"},array{"System","Single[]",&single},rec_class{"Photon.Voice.Unity","Recorder"},manager_class{"","VoiceChatManager"},unity_class{"UnityEngine","Object"},pun_class{"Photon.Voice.PUN","PunVoiceClient"};F pun_field{&pun_class},recorder_field{&rec_class},buffer_field{&array},end_field{&boolean},source_field{&single},record_field{&boolean};
std::map<Handle,Object*> roots;Handle serial=0;N* room_ptr=nullptr;bool frame_end=false;bool stale_audio=false;bool room_voice=false,voice_joined=false,box_value=false;N props{&unity_class},key_node{&unity_class},client{&unity_class},connection{&unity_class};
int start_forwards=0;int frame_forwards=0;bool source_failure=false;std::vector<std::string> actions;
Object* O(void* p){return reinterpret_cast<Object*>(p);}N* Node(Object* p){return reinterpret_cast<N*>(p);}const Method* M(uintptr_t n){return reinterpret_cast<const Method*>(n);}Field* FL(F* p){return reinterpret_cast<Field*>(p);}Class* CL(C* p){return reinterpret_cast<Class*>(p);}
Handle New(Object* o,bool){roots[++serial]=o;return serial;}Object* Get(Handle h){return roots.at(h);}void Free(Handle h){assert(roots.erase(h));}
void OriginalRecord(Object* o,bool v,const Method*){Node(o)->record=v;actions.push_back(v?"record-on":"record-off");}
void OriginalTransmit(Object* o,bool v,const Method*){Node(o)->transmit=v;actions.push_back(v?"transmit-on":"transmit-off");}
void OriginalSource(Object* o,int v,const Method*){if(!source_failure)Node(o)->source=v;actions.push_back("source-"+std::to_string(v));}
void OriginalStart(Object*,const Method*){++start_forwards;}
void OriginalFrame(Object*,Object*,const Method*){++frame_forwards;}
Class* ObjectClass(Object* o){if(o==O(&box_value))return CL(&boolean);return o==O(&buffer)?CL(&array):CL(Node(o)->c);}const char* Name(Class* c){return reinterpret_cast<C*>(c)->name;}const char* Namespace(Class* c){return reinterpret_cast<C*>(c)->ns;}const Type* FieldType(Field* f){return reinterpret_cast<const Type*>(reinterpret_cast<F*>(f)->type);}Class* TypeClass(const Type* t){return reinterpret_cast<Class*>(const_cast<Type*>(t));}
Field* FieldByName(Class* c,const char* name){if(c!=CL(&manager_class))return nullptr;if(!std::strcmp(name,"punVoiceClient"))return FL(&pun_field);return !std::strcmp(name,"recorder")?FL(&recorder_field):nullptr;}
void FieldRead(Object* o,Field* f,void* p){if(f==FL(&source_field))*static_cast<int*>(p)=Node(o)->source;else if(f==FL(&record_field))*static_cast<bool*>(p)=Node(o)->record;else if(f==FL(&pun_field))*static_cast<Object**>(p)=O(&connection);else if(f==FL(&buffer_field)){*static_cast<Object**>(p)=O(&buffer);if(stale_audio)++voice_ai::capture_epoch;}else if(f==FL(&end_field))*static_cast<bool*>(p)=frame_end;else{assert(f==FL(&recorder_field));*static_cast<Object**>(p)=O(Node(o)->recorder);}}
Object* Invoke(const Method* method,void* self,void** args,Object** error){*error=nullptr;auto* n=static_cast<N*>(self);static bool yes=true;switch(reinterpret_cast<uintptr_t>(method)){
case 1:return O(&Node(O(args[0]))->alive);case 2:return O(n->go);case 3:return O(&yes);case 4:return O(room_ptr);
case 5:return O(&n->record);case 6:OriginalRecord(O(n),*static_cast<bool*>(args[0]),nullptr);return nullptr;
case 7:return O(&n->transmit);case 8:OriginalTransmit(O(n),*static_cast<bool*>(args[0]),nullptr);return nullptr;
case 9:return O(&n->source);case 10:OriginalSource(O(n),*static_cast<int*>(args[0]),nullptr);return nullptr;
case 11:return O(n->clip);case 12:n->clip=Node(O(args[0]));return nullptr;
case 13:return O(&n->loop);case 14:n->loop=*static_cast<bool*>(args[0]);return nullptr;
case 15:return O(&n->detection);case 16:n->detection=*static_cast<bool*>(args[0]);return nullptr;
case 17:Node(O(args[0]))->alive=false;return nullptr;
case 18:return O(&props);case 19:box_value=room_voice;return O(&box_value);case 20:return O(&client);case 21:return O(&voice_joined);
default:assert(false);return nullptr;}}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
extern "C" int DobbyHook(void*,void*,void**){return -1;}
namespace chat_bot_http {bool Speak(const std::string&,std::string&){return false;}bool Post(const std::string&,const std::string&,const std::string&,int&,std::string&){return false;}}
int main(){
 Api a{};a.gchandle_new=New;a.gchandle_get_target=Get;a.gchandle_free=Free;a.runtime_invoke=Invoke;a.object_unbox=[](Object* o)->void*{return o;};a.object_get_class=ObjectClass;a.class_get_field_from_name=FieldByName;a.field_get_value=FieldRead;a.field_get_type=FieldType;a.class_from_type=TypeClass;a.class_get_name=Name;a.class_get_namespace=Namespace;a.string_new=[](const char*){return reinterpret_cast<String*>(&key_node);};
 N room{&unity_class},manager{&manager_class},go{&unity_class},rec{&rec_class},remote{&rec_class},old{&unity_class},clip{&unity_class};room_ptr=&room;manager.go=&go;manager.recorder=&rec;rec.clip=&old;
 voice_ai::mic_api=&a;voice_ai::mic_source_field=FL(&source_field);voice_ai::mic_record_field=FL(&record_field);voice_ai::original_start=OriginalStart;voice_ai::original_record=OriginalRecord;voice_ai::original_transmit=OriginalTransmit;voice_ai::original_source=OriginalSource;
 {
 voice_ai::Runtime r(a);r.alive=M(1);r.go=M(2);r.active=M(3);r.current_room=M(4);r.destroy=M(17);r.manager.Set(O(&manager));r.recorder.Set(O(&rec));r.room_id=reinterpret_cast<uintptr_t>(O(&room));r.hooks=true;
 r.get_record=M(5);r.set_record=M(6);r.get_transmit=M(7);r.set_transmit=M(8);r.get_source=M(9);r.set_source=M(10);r.get_clip=M(11);r.set_clip=M(12);r.get_loop=M(13);r.set_loop=M(14);r.get_detect=M(15);r.set_detect=M(16);
 r.room_properties=M(18);r.hash_item=M(19);r.voice_client=M(20);r.voice_joined=M(21);r.available=M(3);
 connection.c=&pun_class;room_voice=false;voice_joined=true;assert(!r.NetworkAllowed());room_voice=true;voice_joined=false;assert(!r.NetworkAllowed());voice_joined=true;assert(r.NetworkAllowed()); // availability alone is not a joined voice room
 r.clip.Set(O(&clip));assert(r.AcquireMic(true));assert(rec.source==1&&rec.clip==&clip&&rec.record&&rec.transmit&&!rec.loop&&!rec.detection&&r.snap.mic_blocked);
 assert(actions[0]=="transmit-off"&&actions[1]=="record-off");
 voice_ai::SourceHook(O(&rec),0,nullptr);voice_ai::RecordHook(O(&rec),false,nullptr);voice_ai::TransmitHook(O(&rec),false,nullptr);
 assert(rec.source==1&&rec.record&&rec.transmit); // game cannot reselect mic during AI clip
 voice_ai::RecordHook(O(&remote),false,nullptr);assert(!remote.record&&rec.record); // another recorder forwards unchanged
 r.StopAudio();assert(rec.source==0&&rec.clip==&old&&!rec.record&&!rec.transmit&&rec.loop&&rec.detection&&!r.lease&&voice_ai::leased_recorder==nullptr); // latest mute preserved
 rec.record=rec.transmit=true;clip.alive=true;r.clip.Set(O(&clip));assert(r.AcquireMic(false));assert(!rec.record&&!rec.transmit&&rec.source==0); // local voice also isolates mic
 voice_ai::RecordHook(O(&rec),true,nullptr);assert(!rec.record);r.StopAudio();assert(rec.record&&rec.transmit&&rec.source==0);
 // A second/new microphone cannot start during local AI playback; its latest requested state restores.
 remote.go=&go;remote.record=true;clip.alive=true;r.clip.Set(O(&clip));assert(r.AcquireMic(false));
 voice_ai::mic_guard_active=true;voice_ai::RememberMic(O(&remote));voice_ai::RecordHook(O(&remote),true,nullptr);assert(!remote.record);
 voice_ai::StartHook(O(&remote),nullptr);voice_ai::StartHook(O(&rec),nullptr);assert(start_forwards==0);
 voice_ai::RecordHook(O(&remote),false,nullptr);r.StopAudio();assert(!remote.record&&rec.record&&!voice_ai::mic_guard_active&&voice_ai::extra_mics.empty());
 voice_ai::StartHook(O(&remote),nullptr);assert(start_forwards==1);
 clip.alive=true;r.clip.Set(O(&clip));assert(r.AcquireMic(false));voice_ai::mic_guard_active=true;voice_ai::RecordHook(O(&remote),true,nullptr);assert(!remote.record);r.StopAudio();assert(remote.record&&voice_ai::extra_mics.empty());
 clip.alive=true;r.clip.Set(O(&clip));source_failure=true;assert(!r.AcquireMic(true));assert(!rec.record&&!rec.transmit);source_failure=false;r.StopAudio();assert(rec.source==0&&rec.record&&rec.transmit);
 clip.alive=true;r.clip.Set(O(&clip));assert(r.AcquireMic(true));N next_room{&unity_class};room_ptr=&next_room;r.StopAudio();assert(!rec.record&&!rec.transmit); // stale room never reopens microphone
 room_ptr=&room;
 }
 assert(roots.empty());
 a.array_length=[](Object*)->uintptr_t{return 6400;};a.array_object_header_size=[]()->uint32_t{return offsetof(Buffer,data);};a.array_element_size=[](Class*)->int32_t{return 4;};a.class_get_element_class=[](Class* c){return CL(reinterpret_cast<C*>(c)->element);};
 voice_ai::float_class=CL(&single);voice_ai::hook_api=&a;voice_ai::buf_field=FL(&buffer_field);voice_ai::eos_field=FL(&end_field);voice_ai::original_frame=OriginalFrame;
 N link{&unity_class},frame{&unity_class};for(float& f:buffer.data)f=.1f;
 {auto list=std::make_shared<voice_ai::Streams>();list->push_back(std::make_shared<voice_ai::Stream>(a,O(&link),2,16000,1,"Ali"));std::atomic_store(&voice_ai::streams,std::shared_ptr<const voice_ai::Streams>(list));}
 voice_ai::capture=true;voice_ai::FrameHook(O(&link),O(&frame),nullptr);voice_ai::segments.Flush(voice_ai::Now()+1);voice_ai_core::Utterance u;assert(voice_ai::segments.Take(u)&&u.actor==2&&u.samples.size()==6400&&frame_forwards==1);
 stale_audio=true;voice_ai::capture=true;voice_ai::FrameHook(O(&link),O(&frame),nullptr);voice_ai::segments.Flush(voice_ai::Now()+2);assert(!voice_ai::segments.Take(u));stale_audio=false;
 voice_ai::capture=false;voice_ai::FrameHook(O(&link),O(&frame),nullptr);assert(frame_forwards==3);voice_ai::segments.Flush(voice_ai::Now()+2);assert(!voice_ai::segments.Take(u));
 std::atomic_store(&voice_ai::streams,std::shared_ptr<const voice_ai::Streams>(std::make_shared<voice_ai::Streams>()));assert(roots.empty());
 std::cout<<"Voice native runtime: mic shutoff before clip, direct clip source/readback, external mic re-enable blocked, remembered mute restore, local playback mute, setup failure and stale-room fail-closed, pinned PCM with runtime array header and original frame forwarding: PASS\n";
}
