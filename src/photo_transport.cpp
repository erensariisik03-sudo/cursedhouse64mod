#include "photo_transport.h"
#ifdef __ANDROID__
#include "photo_transfer_bridge.h"
#include "chat_bot_bridge.h"
#include "storage_access.h"
#include "photo_transport_bridge.h"
#include "kick_bridge.h"
#include "kick_protocol.h"
#include "photo_transfer_protocol.h"
#include "photo_browse_protocol.h"
#include "dobby.h"
#include <android/log.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <deque>
#include <iterator>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <sys/stat.h>
#include <utility>

#define PHOTO_LOG(...) __android_log_print(ANDROID_LOG_INFO,"multiplayermod",__VA_ARGS__)
namespace photo_transport {namespace {
using namespace il2cpp;
using namespace photo_transfer_security;
namespace wire=photo_transfer_protocol;
namespace browse=photo_browse_protocol;
namespace bridge=photo_transport_bridge;
struct Root {
 const Api& a;Handle h=0;explicit Root(const Api& api,Object* p=nullptr):a(api){if(p)h=a.gchandle_new(p,false);}
 ~Root(){if(h)a.gchandle_free(h);}Object* Get()const{return h?a.gchandle_get_target(h):nullptr;}
 Root(const Root&)=delete;Root& operator=(const Root&)=delete;
};
using EventFn=void (*)(Object*,Object*,const Method*);
EventFn original_event=nullptr;
const Api* event_api=nullptr;
Field* event_code_field=nullptr;
const Method *event_indexer=nullptr,*event_data=nullptr,*event_code_get=nullptr,*event_sender_get=nullptr;
struct Message {int sender=0;std::string data;};
std::mutex inbox_mutex;std::deque<Message> inbox;
void EventHook(Object* client,Object* event,const Method* method) {
 // Read EventData *before* forwarding. Photon may recycle/clear event payloads
 // after callbacks. Only copy a bounded string; process state on Unity Update.
 try {
  if(event_api&&event&&(event_code_field||event_code_get)&&(event_indexer||event_sender_get)&&event_data) {
   const Api& a=*event_api;
   uint8_t code=0;
   if(event_code_field)a.field_get_value(event,event_code_field,&code);
   else code=a.Value<uint8_t>(event_code_get,event,0);
   if(code==wire::kEventCode){
    bool ok=false;
    int actor=event_sender_get?a.Value<int>(event_sender_get,event,0):0;
    if(actor<=0&&event_indexer){
     uint8_t sender_key=254;void* args[]={&sender_key};
     Root sender(a,a.Call(event_indexer,event,args,&ok));
     if(ok&&sender.Get()&&a.object_get_class){
      auto* sender_type=a.object_get_class(sender.Get());
      const bool is_int=sender_type&&!std::strcmp(a.class_get_name(sender_type),"Int32")&&
                              !std::strcmp(a.class_get_namespace(sender_type),"System");
      if(is_int)actor=*static_cast<int*>(a.object_unbox(sender.Get()));
     }
    }
    if(actor<=0) PHOTO_LOG("GMM photo RX: Photon event code 184 but sender actor missing");
    if(actor>0){
     Root data(a,a.Call(event_data,event,nullptr,&ok));
      if(ok&&data.Get()&&a.object_get_class){
       auto* klass=a.object_get_class(data.Get());
       if(klass&&!std::strcmp(a.class_get_name(klass),"String")&&
          !std::strcmp(a.class_get_namespace(klass),"System")){
        std::string line=a.Utf8(reinterpret_cast<String*>(data.Get()));
        if(line.size()<=1800&&(line.compare(0,7,"GMMIMG1")==0||line.compare(0,7,"GMMIMG2")==0||line.compare(0,7,"GMMKCK1")==0)){
         // Control events only: never log photo bytes or filenames.
         if(line.size()>9 && line[8]!='C')
           PHOTO_LOG("GMM photo RX: protocol=%c kind=%c sender=%d",line[6],line[8],actor);
         std::lock_guard<std::mutex> lock(inbox_mutex);
         if(inbox.size()<256)inbox.push_back({actor,std::move(line)});
         else PHOTO_LOG("GMM photo RX: incoming queue full; packet discarded");
        }
       }
      }
     }
    }
   }
 } catch(...) {/* bad or reused event must never crash the game */}
 if(original_event)original_event(client,event,method);
}

static double Now() {return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static const std::string kShareDir="/sdcard/Pictures";
class Session {
 const Api& a;
 const Method *raise{},*client_get{},*op_raise{},*opt_ctor{},*current_room{},*local_player{},*players{},*actor_get{},*nick_get{},*own_nick{},*array_length{},*array_item{},*leave_room{};
 Class *opt_class{},*send_class{},*int_class{};
 Field *target_actors{},*reliable{},*encrypted{};
 const Method* send_reliable_get{};
 const size_t max_send_size=32;
 bool valid=false,installed=false;
 double next_diagnostic=0;
 uintptr_t room_id=0;
 double next_send=0,next_poll=0,incoming_deadline=0,outgoing_deadline=0;
 bridge::Snapshot snap;
 int local_actor=0,requested_sender=0,outgoing_peer=0;
 Offer incoming_offer{};
 Reassembler rx;
 Bytes tx_data;
 size_t tx_index=0;
 uint64_t outgoing_id=0;
 bool tx_accepted=false;
 int pending_actor=0;
 // Local, explicit opt-in for photo sharing during the CURRENT room only.
 // No remote packet can turn this on. It resets on room exit/change.
 bool sharing_enabled=false;
 double next_policy_check=0,next_kick_allowed=0;
 kick_bridge::Snapshot kick_snapshot;
 photo_transfer_security::Bytes admin_key;
 std::deque<std::pair<int,uint64_t>> seen_kicks;
 std::string pending_file;
 std::vector<bridge::Peer> peers;
 std::string private_files_dir;
 std::string last_saved_path;
 // One-time, per-room browse authorization. The data owner must tap Accept
 // before any filename is listed; only direct /Pictures JPG/PNG files can be read.
 uint64_t share_request_id=0,approved_id=0,browse_id=0;
 int share_request_actor=0,approved_actor=0,browse_actor=0;
 double share_deadline=0,approval_deadline=0,browse_deadline=0,next_browse_send=0;
 bool browse_granted=false,browse_fetching=false;
 std::vector<std::string> approved_files;
 std::deque<std::pair<int,std::string>> browse_outbox;
 std::string selected_remote_file;
 std::vector<std::string> selections;
 std::deque<std::string> batch;
 size_t batch_finished=0;
 std::string sharing_with;

 const Method* M(const char* ns,const char* cls,const char* name,int n=0){return a.FindMethod(a.Find(ns,cls),name,n);}
 bool IsPeer(int actor)const {return actor>0&&std::any_of(peers.begin(),peers.end(),[&](const auto& p){return p.actor==actor;});}
 void Note(std::string s){snap.status=std::move(s);}
 void ResetBrowse(){
  share_request_id=approved_id=browse_id=0;share_request_actor=approved_actor=browse_actor=0;sharing_enabled=false;
  share_deadline=approval_deadline=browse_deadline=next_browse_send=0;
  browse_granted=browse_fetching=false;approved_files.clear();browse_outbox.clear();selected_remote_file.clear();
  selections.clear();batch.clear();sharing_with.clear();batch_finished=0;
  snap.share_request=snap.browsing=snap.listing_ready=snap.fetching=false;
  snap.share_request_from.clear();snap.remote_files.clear();snap.selected_remote_file.clear();
 }
 void ResetSession(){
  rx.Cancel();incoming_offer={};requested_sender=0;outgoing_id=0;outgoing_peer=0;tx_data.clear();tx_index=0;tx_accepted=false;
  incoming_deadline=outgoing_deadline=0;snap.incoming=snap.sending=false;snap.incoming_from.clear();snap.progress=snap.total=0;
 }
 bool Resolve() {
  if(!a.array_new||!a.array_length||!a.array_element_size||!a.runtime_class_init||!a.array_object_header_size||!a.class_from_type||!a.class_get_field_from_name||!a.field_set_value_object||!a.object_get_class||!a.object_new||!a.field_static_get_value||!a.field_get_value||!a.field_set_value||!a.class_value_size){
   PHOTO_LOG("GMM photo resolve: missing required IL2CPP native API");return false;
  }
  raise=M("Photon.Pun","PhotonNetwork","RaiseEvent",4);
  // Some game builds wrap/override PUN's static RaiseEvent. The documented
  // Realtime client has the same explicit four-argument operation.
  client_get=M("Photon.Pun","PhotonNetwork","get_NetworkingClient");
  op_raise=M("Photon.Realtime","LoadBalancingClient","OpRaiseEvent",4);
  current_room=M("Photon.Pun","PhotonNetwork","get_CurrentRoom");
  leave_room=M("Photon.Pun","PhotonNetwork","LeaveRoom",1);
  local_player=M("Photon.Pun","PhotonNetwork","get_LocalPlayer");
  players=M("Photon.Pun","PhotonNetwork","get_PlayerListOthers");
  actor_get=M("Photon.Realtime","Player","get_ActorNumber");
  nick_get=M("Photon.Realtime","Player","get_NickName");
  own_nick=M("Photon.Pun","PhotonNetwork","get_NickName");
  array_length=M("System","Array","get_Length");
  array_item=M("System","Array","GetValueImpl",1);
  opt_class=a.Find("Photon.Realtime","RaiseEventOptions");
  // il2cpp_object_new only allocates: it does NOT run the managed .ctor.
  // Photon RaiseEventOptions.ctor sets Flags = WebFlags.Default.
  // Skipping this can cause NullReferenceException in OpRaiseEvent.
  opt_ctor=a.FindMethod(opt_class,".ctor",0);
  send_class=a.Find("ExitGames.Client.Photon","SendOptions");
  int_class=a.Find("System","Int32");
  target_actors=opt_class?a.class_get_field_from_name(opt_class,"TargetActors"):nullptr;
  reliable=send_class?a.class_get_field_from_name(send_class,"SendReliable"):nullptr;
  encrypted=send_class?a.class_get_field_from_name(send_class,"Encrypt"):nullptr;
  send_reliable_get=a.FindMethod(send_class,"get_SendReliable",0);
  auto* ev=a.Find("ExitGames.Client.Photon","EventData");
  event_code_field=ev?a.class_get_field_from_name(ev,"Code"):nullptr;
  event_code_get=a.FindMethod(ev,"get_Code",0);
  event_sender_get=a.FindMethod(ev,"get_Sender",0);
  event_indexer=a.FindMethod(ev,"get_Item",1);
  event_data=a.FindMethod(ev,"get_CustomData",0);
  auto* client=a.Find("Photon.Realtime","LoadBalancingClient");
  auto* on_event=a.FindMethod(client,"OnEvent",1);
  if(!raise||!current_room||!local_player||!players||!actor_get||!nick_get||!array_length||!array_item||
     !opt_class||!opt_ctor||!send_class||!int_class||!target_actors||!encrypted||(!reliable&&!send_reliable_get)||
     (!event_code_field&&!event_code_get)||(!event_indexer&&!event_sender_get)||!event_data||!on_event||!Entry(on_event)){
   PHOTO_LOG("GMM photo resolve: methods raise=%d room=%d me=%d peers=%d actor=%d nick=%d len=%d item=%d options=%d ctor=%d send=%d int=%d targets=%d enc=%d reliable=%d code=%d indexer=%d data=%d hook=%d",
    !!raise,!!current_room,!!local_player,!!players,!!actor_get,!!nick_get,!!array_length,!!array_item,
    !!opt_class,!!opt_ctor,!!send_class,!!int_class,!!target_actors,!!encrypted,!!reliable||!!send_reliable_get,
    !!event_code_field||!!event_code_get,!!event_indexer||!!event_sender_get,!!event_data,!!on_event&&!!Entry(on_event));
   return false;
  }
  uint32_t alignment=0;const int send_size=a.class_value_size(send_class,&alignment);
  if(send_size<=0||send_size>int(max_send_size)||alignment>alignof(std::max_align_t)){
   PHOTO_LOG("GMM photo resolve: SendOptions ABI size=%d align=%u",send_size,alignment);return false;
  }
  a.runtime_class_init(send_class);
  a.runtime_class_init(opt_class);
  event_api=&a;
  installed=DobbyHook(Entry(on_event),reinterpret_cast<void*>(EventHook),reinterpret_cast<void**>(&original_event))==0 && original_event;
  if(!installed)PHOTO_LOG("GMM photo resolve: Photon OnEvent hook installation failed");
  return installed;
 }
 // Distinguish metadata resolution from an exception thrown inside Photon.
 // Never log managed exception messages, photo data, or file names.
 bool InvokeSend(const Method* method,Object* self,void** args,const char* path,bool& invoked){
  invoked=false;
  if(!method)return false;
  Object* exception=nullptr;
  Root result(a,a.runtime_invoke(method,self,args,&exception));
  if(exception){
   auto* klass=a.object_get_class(exception);
   const char* ns=klass?a.class_get_namespace(klass):"?";
   const char* name=klass?a.class_get_name(klass):"?";
   PHOTO_LOG("GMM photo TX exception via=%s class=%s.%s",path,ns?ns:"?",name?name:"?");
   return false;
  }
  invoked=true;
  if(!result.Get()){
   PHOTO_LOG("GMM photo TX empty result via=%s",path);return false;
  }
  auto* klass=a.object_get_class(result.Get());
  const char* ns=klass?a.class_get_namespace(klass):nullptr;
  const char* name=klass?a.class_get_name(klass):nullptr;
  if(!ns||!name||std::strcmp(ns,"System")||std::strcmp(name,"Boolean")){
   PHOTO_LOG("GMM photo TX unexpected result via=%s",path);return false;
  }
  return *static_cast<bool*>(a.object_unbox(result.Get()));
 }
 bool SendEvent(int recipient,const std::string& text){
  if(!valid||!IsPeer(recipient)||text.empty()||text.size()>1800){
   PHOTO_LOG("GMM photo send blocked: ready=%d peer=%d target=%d payload_length=%zu",valid,IsPeer(recipient),recipient,text.size());return false;
  }
  Root option(a,a.object_new(opt_class));
  if(!option.Get())return false;
  bool constructor_ok=false;
  a.Call(opt_ctor,option.Get(),nullptr,&constructor_ok);
  if(!constructor_ok){
   PHOTO_LOG("GMM photo TX: RaiseEventOptions constructor failed");
   return false;
  }
  // Validate the non-null reference that the managed constructor sets.
  if(Field* flags=a.class_get_field_from_name(opt_class,"Flags")){
   Object* initialized_flags=nullptr;
   a.field_get_value(option.Get(),flags,&initialized_flags);
   if(!initialized_flags){
    PHOTO_LOG("GMM photo TX: RaiseEventOptions.Flags null after constructor");
    return false;
   }
  }
  Root target(a,a.array_new(int_class,1));
  if(!target.Get()||a.array_length(target.Get())!=1)return false;
  auto* target_class=a.object_get_class(target.Get());
  if(!target_class||a.array_element_size(target_class)!=4)return false;
  const size_t header=a.array_object_header_size();
  if(header<24||header>128||header%4)return false;
  std::memcpy(reinterpret_cast<uint8_t*>(target.Get())+header,&recipient,sizeof(recipient));
  a.field_set_value_object(option.Get(),target_actors,target.Get());
  Root settings(a,a.object_new(send_class));if(!settings.Get())return false;
  uint32_t align=0;const int n=a.class_value_size(send_class,&align);
  if(n<=0||n>int(max_send_size))return false;
  // SendReliable plus Photon client-server encryption; not end-to-end crypto.
  if(reliable){a.field_static_get_value(reliable,a.object_unbox(settings.Get()));}
  else {Root predefined(a,a.Call(send_reliable_get));if(!predefined.Get())return false;std::memcpy(a.object_unbox(settings.Get()),a.object_unbox(predefined.Get()),n);}
  bool encrypt=true;
  a.field_set_value(settings.Get(),encrypted,&encrypt);
  bool verify=false;a.field_get_value(settings.Get(),encrypted,&verify);
  if(!verify)return false;
  Root payload(a,reinterpret_cast<Object*>(a.string_new(text.c_str())));
  if(!payload.Get())return false;
  const uint8_t event=wire::kEventCode;
  void* args[]={const_cast<uint8_t*>(&event),payload.Get(),option.Get(),a.object_unbox(settings.Get())};
  bool invoked=false;
  bool success=InvokeSend(raise,nullptr,args,"PhotonNetwork.RaiseEvent",invoked);
  bool fallback_attempted=false,fallback_invoked=false;
  // A failed enqueue or managed exception is safe to retry on the SDK client.
  // Do not retry after success, otherwise the receiving player gets duplicates.
  if(!success&&client_get&&op_raise){
   bool client_ok=false;
   Root client(a,a.Call(client_get,nullptr,nullptr,&client_ok));
   if(client_ok&&client.Get()){
    fallback_attempted=true;
    success=InvokeSend(op_raise,client.Get(),args,"LoadBalancingClient.OpRaiseEvent",fallback_invoked);
    if(success)PHOTO_LOG("GMM photo TX recovered with Realtime client path");
   }else PHOTO_LOG("GMM photo TX Realtime client not available");
  }
  if(text.size()>9 && text[8]!='C')
   PHOTO_LOG("GMM photo TX: protocol=%c kind=%c target=%d ok=%d",text[6],text[8],recipient,success);
  if(!success)PHOTO_LOG("GMM photo SendEvent failed: invoke=%d fallback_available=%d fallback_attempted=%d fallback_invoked=%d",invoked,!!op_raise,fallback_attempted,fallback_invoked);
  return success;
 }
 bool SharingPolicyEnabled(){
  // Owner-local explicit one-time opt-in. No GUI/share request popup.
  // A remote Photon packet is never allowed to write this file or toggle it.
  if(!storage_access::Granted())return false;
  std::ifstream f("/sdcard/gmmod/pictures_share.cfg");
  std::string line;return bool(std::getline(f,line))&&line=="GMM_PICTURES_SHARE=ON";
 }
 void LoadAdminKey(){
  admin_key.clear();
  // Only devices provisioned by their owner accept/issue a kick command.
  std::ifstream f("/sdcard/gmmod/admin_key.hex");
  std::string key;std::getline(f,key);
  if(key.size()==64){photo_transfer_security::Bytes decoded;
   if(photo_transfer_security::Unhex(key,decoded)&&decoded.size()==32)admin_key=std::move(decoded);
  }
  kick_snapshot.key_ready=admin_key.size()==32&&leave_room!=nullptr;
 }
 void KickIncoming(const Message& msg){
  if(!room_id||!IsPeer(msg.sender)||!local_actor||!leave_room)return;
  uint64_t nonce=0;
  if(!kick_protocol::Decode(msg.data,msg.sender,local_actor,admin_key,nonce)){
   PHOTO_LOG("GMM kick RX: signature invalid sender=%d",msg.sender);return;
  }
  for(const auto& item:seen_kicks)if(item.first==msg.sender&&item.second==nonce)return;
  if(seen_kicks.size()>=128)seen_kicks.pop_front();
  seen_kicks.push_back({msg.sender,nonce});
  bool become_inactive=false;void* args[]={&become_inactive};bool invoked=false;
  Root result(a,a.Call(leave_room,nullptr,args,&invoked));
  const bool accepted=invoked&&result.Get()&&*static_cast<bool*>(a.object_unbox(result.Get()));
  PHOTO_LOG("GMM kick RX: authenticated sender=%d leave_accepted=%d",msg.sender,accepted);
  if(accepted)storage_access::ShowToast("Admin sizi banladı");
 }
 void HandleKickCommands(){
  const auto cmd=kick_bridge::Take();
  if(cmd.choose_actor>0&&IsPeer(cmd.choose_actor))kick_snapshot.selected_actor=cmd.choose_actor;
  kick_snapshot.in_room=room_id!=0;
  kick_snapshot.peers.clear();
  for(const auto& peer:peers)kick_snapshot.peers.push_back({peer.actor,peer.display});
  if(!IsPeer(kick_snapshot.selected_actor))kick_snapshot.selected_actor=0;
  if(cmd.kick){
   const int target=kick_snapshot.selected_actor;
   if(!chat_bot_bridge::Read().developer_unlocked){kick_snapshot.status="Gelistirici kilidi acik olmali.";}
   else if(admin_key.size()!=32||!leave_room){kick_snapshot.status="Yonetici anahtari eksik.";}
   else if(!IsPeer(target)){kick_snapshot.status="Odadaki bir oyuncuyu sec.";}
   else if(Now()<next_kick_allowed){kick_snapshot.status="Yeniden denemek icin biraz bekle.";}
   else{
    next_kick_allowed=Now()+3.0;
    std::random_device rng;
    uint64_t nonce=(uint64_t(rng())<<32)|uint64_t(rng());if(!nonce)nonce=1;
    const auto packet=kick_protocol::Encode(nonce,local_actor,target,admin_key);
    const bool ok=!packet.empty()&&SendEvent(target,packet);
    kick_snapshot.status=ok?"Kickle komutu gonderildi; hedefin cikisi henuz dogrulanmadi.":"Kickle komutu gonderilemedi.";
    PHOTO_LOG("GMM kick TX: target=%d enqueued=%d",target,ok);
   }
  }
  kick_bridge::Publish(kick_snapshot);
 }
 void DiscoverRoom(){
  Root room(a,a.Call(current_room));const uintptr_t id=reinterpret_cast<uintptr_t>(room.Get());
  if(room_id&&room_id!=id){
   ResetSession();ResetBrowse();pending_actor=0;pending_file.clear();seen_kicks.clear();
   kick_snapshot.selected_actor=0;
   std::lock_guard<std::mutex> lock(inbox_mutex);inbox.clear();
  }
  room_id=id;
  peers.clear();local_actor=0;snap.local_name.clear();
  if(!room.Get())return;
  Root me(a,a.Call(local_player));
  local_actor=a.Value<int>(actor_get,me.Get(),0);
  if(own_nick){Root own(a,a.Call(own_nick));if(own.Get())snap.local_name=a.Utf8(reinterpret_cast<String*>(own.Get()));}
  Root others(a,a.Call(players));
  if(!others.Get())return;
  const int count=a.Value<int>(array_length,others.Get(),-1);
  if(count<0||count>64)return;
  for(int i=0;i<count;i++){
   void* args[]={&i};Root player(a,a.Call(array_item,others.Get(),args));
   if(!player.Get())continue;
   const int id=a.Value<int>(actor_get,player.Get(),0);
   if(id<=0||id==local_actor)continue;
   Root nick(a,a.Call(nick_get,player.Get()));
   std::string label=a.Utf8(reinterpret_cast<String*>(nick.Get()));
   peers.push_back({id,label.empty()?std::to_string(id):std::move(label)});
  }
 }
 std::vector<std::string> LocalImages() {
  std::vector<std::string> files;
  mkdir("/sdcard/Pictures",0755);
  DIR* dir=opendir(kShareDir.c_str());if(!dir)return files;
  for(dirent* entry;(entry=readdir(dir))!=nullptr && files.size()<100;){
   const std::string name=entry->d_name;
   if(!browse::NameOk(name))continue;
   const auto dot=name.find_last_of('.');if(dot==std::string::npos)continue;
   std::string ext=name.substr(dot);std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
   if(ext!=".jpg"&&ext!=".jpeg"&&ext!=".png")continue;
   struct stat st{};const std::string full=kShareDir+"/"+name;
   if(lstat(full.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_size>=8&&st.st_size<=static_cast<off_t>(kMaxImageBytes))files.push_back(name);
  }
  closedir(dir);std::sort(files.begin(),files.end());return files;
 }
 bool LoadChosen(const std::string& filename,Bytes& bytes){
  bytes.clear();
  if(!browse::NameOk(filename))return false;
  const std::string path=kShareDir+"/"+filename;
  // Open the exact approved file without following symlinks (no lstat/open race).
  const int fd=open(path.c_str(),O_RDONLY|O_NOFOLLOW);
  if(fd<0)return false;
  struct stat st{};
  const bool allowed=fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_size>=8&&st.st_size<=static_cast<off_t>(kMaxImageBytes);
  if(!allowed){close(fd);return false;}
  bytes.resize(static_cast<size_t>(st.st_size));size_t read_bytes=0;bool ok=true;
  while(read_bytes<bytes.size()){
   const ssize_t n=read(fd,bytes.data()+read_bytes,bytes.size()-read_bytes);
   if(n<0&&errno==EINTR)continue;
   if(n<=0){ok=false;break;}
   read_bytes+=size_t(n);
  }
  if(close(fd)!=0)ok=false;
  if(!ok||!IsImage(bytes)){bytes.clear();return false;}
  return true;
 }
 bool SaveReceived(const Bytes& bytes,uint64_t id){
  last_saved_path.clear();
  if(!IsImage(bytes))return false;
  const bool png=bytes.size()>=8&&bytes[0]==137&&bytes[1]==80;
  // First try MediaStore; the emulator can write its own new Pictures item
  // even when it has not enabled all-files access.
  if(storage_access::SaveImageToGallery(bytes.data(),bytes.size(),png,id,last_saved_path)){
   PHOTO_LOG("GMM photo save: MediaStore success bytes=%zu",bytes.size());return true;
  }
  PHOTO_LOG("GMM photo save: MediaStore unavailable, trying filesystem");
  std::string private_dir;
  if(private_files_dir.empty()){
   std::ifstream cmd("/proc/self/cmdline",std::ios::binary);std::string pkg;std::getline(cmd,pkg,'\0');
   if(!pkg.empty()&&pkg.find('/')==std::string::npos){
    const std::string base="/data/user/0/"+pkg+"/files";
    struct stat st{};if(stat(base.c_str(),&st)==0&&S_ISDIR(st.st_mode))private_files_dir=base;
   }
  }
  if(!private_files_dir.empty())private_dir=private_files_dir+"/GMMReceived";
  const auto save_dir=[&](const std::string& dest)->bool{
   if(dest.empty())return false;
   if(mkdir(dest.c_str(),0700)!=0&&errno!=EEXIST){
    PHOTO_LOG("GMM photo save: mkdir failed errno=%d",errno);return false;
   }
   struct stat dir_stat{};
   if(lstat(dest.c_str(),&dir_stat)!=0||!S_ISDIR(dir_stat.st_mode))return false;
   for(unsigned serial=0;serial<10000;serial++){
    const std::string path=dest+"/received_"+std::to_string(id)+"_"+std::to_string(serial)+(png?".png":".jpg");
    const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(fd<0){if(errno==EEXIST)continue;PHOTO_LOG("GMM photo save: open failed errno=%d",errno);return false;}
    size_t written=0;bool ok=true;
    while(written<bytes.size()){
     const ssize_t n=write(fd,bytes.data()+written,bytes.size()-written);
     if(n<0&&errno==EINTR)continue;
     if(n<=0){ok=false;break;}
     written+=size_t(n);
    }
    if(close(fd)!=0)ok=false;
    if(!ok){unlink(path.c_str());PHOTO_LOG("GMM photo save: write failed");return false;}
    last_saved_path=path;PHOTO_LOG("GMM photo save: filesystem success bytes=%zu",bytes.size());return true;
   }
   return false;
  };
  if(storage_access::Granted()){
   mkdir("/sdcard/Pictures",0755);
   if(save_dir("/sdcard/Pictures/GMMReceived"))return true;
  }
  if(save_dir(private_dir))return true;
  PHOTO_LOG("GMM photo save: all destinations failed");return false;
 }

 // Sharing is enabled locally for the current Photon room, not by a packet.
 // Any peer in that room may request a listing while the owner has sharing enabled.
 bool GrantBrowseToRoom(int actor,uint64_t id,double now){
  if(!room_id||!sharing_enabled||!IsPeer(actor))return false;
  auto files=LocalImages();
  if(!SendEvent(actor,browse::Encode(browse::Kind::Grant,id,actor)))return false;
  approved_id=id;approved_actor=actor;approval_deadline=now+900;
  approved_files=std::move(files);
  sharing_with=std::to_string(actor);
  for(const auto& peer:peers)if(peer.actor==actor)sharing_with=peer.display;
  for(const auto& name:approved_files)
   browse_outbox.emplace_back(actor,browse::Encode(browse::Kind::Listing,id,actor,name));
  browse_outbox.emplace_back(actor,browse::Encode(browse::Kind::End,id,actor));
  PHOTO_LOG("GMM photo owner: locally enabled room share granted actor=%d files=%zu",actor,approved_files.size());
  return true;
 }
 void IncomingBrowse(const Message& msg){
  browse::Packet p;if(!browse::Decode(msg.data,p)||!IsPeer(msg.sender)||!local_actor||p.recipient!=local_actor)return;
  const double now=Now();
  switch(p.kind){
   case browse::Kind::Request: {
    // No per-request popup. The owner must first enable sharing locally for
    // this room. Without it requests are denied before any filenames leak.
    if(!room_id||!sharing_enabled||outgoing_id){
     SendEvent(msg.sender,browse::Encode(browse::Kind::Deny,p.id,msg.sender));break;
    }
    // Allow a refreshed listing only while owner-enabled room sharing is on,
    // and do not interrupt a currently sending photo.
    approved_id=0;approved_actor=0;approval_deadline=0;
    approved_files.clear();browse_outbox.clear();sharing_with.clear();
    if(!GrantBrowseToRoom(msg.sender,p.id,now))
     SendEvent(msg.sender,browse::Encode(browse::Kind::Deny,p.id,msg.sender));
    break;
   }
   case browse::Kind::Grant:
    if(browse_id==p.id&&browse_actor==msg.sender){browse_granted=true;Note("Oyuncu izin verdi. Fotograf listesi geliyor...");}
    break;
   case browse::Kind::Deny:
    if(browse_id==p.id&&browse_actor==msg.sender){
      const bool was_fetching=browse_fetching;
      browse_id=0;browse_actor=0;browse_granted=browse_fetching=false;
      snap.browsing=snap.listing_ready=snap.fetching=false;
      snap.remote_files.clear();selected_remote_file.clear();selections.clear();batch.clear();
      Note(was_fetching?"Karsi cihaz secilen fotografi acamadi ya da paylasim suresi doldu.":
                       "Oyuncu paylasim istegini reddetti veya oturum iptal edildi.");
    }
    // Requester can revoke a pending or previously approved share session.
    if(share_request_id==p.id&&share_request_actor==msg.sender){
      share_request_id=0;share_request_actor=0;share_deadline=0;
      snap.share_request=false;snap.share_request_from.clear();
    }
    if(approved_id==p.id&&approved_actor==msg.sender){
      approved_id=0;approved_actor=0;approval_deadline=0;
      approved_files.clear();browse_outbox.clear();sharing_with.clear();
    }
    break;
   case browse::Kind::Listing:
    if(browse_id==p.id&&browse_actor==msg.sender&&browse_granted&&!snap.listing_ready&&
       snap.remote_files.size()<100&&std::find(snap.remote_files.begin(),snap.remote_files.end(),p.name)==snap.remote_files.end())
      snap.remote_files.push_back(p.name);
    break;
   case browse::Kind::End:
    if(browse_id==p.id&&browse_actor==msg.sender&&browse_granted){
      snap.listing_ready=true;
      // The common case is a single approved image. Make the fetch button
      // useful without a separate row tap; do NOT choose arbitrarily if
      // multiple files were shared.
      if(snap.remote_files.size()==1){
       selected_remote_file=snap.remote_files.front();selections={selected_remote_file};
       Note("Tek fotograf otomatik secildi. 'Secilen fotografi al' dugmesine bas.");
      }else Note(snap.remote_files.empty()?"Paylasilan klasorde uygun fotograf yok.":"Paylasilan fotograflardan birini sec.");
      PHOTO_LOG("GMM photo browse listing complete count=%zu auto_selected=%d",snap.remote_files.size(),!selected_remote_file.empty());
    }
    break;
   case browse::Kind::Fetch: {
    // Browsing approval is one-session and limited to its approved file set.
    // A stale/unreadable file previously failed silently, leaving the
    // requester's UI stuck at "fetching" for up to two minutes.
    const bool same_session=approved_id==p.id&&approved_actor==msg.sender;
    const bool allowed=same_session&&now<approval_deadline&&!outgoing_id&&
       std::find(approved_files.begin(),approved_files.end(),p.name)!=approved_files.end();
    PHOTO_LOG("GMM photo owner: fetch received approved=%d allowed=%d sender=%d",same_session,allowed,msg.sender);
    if(!allowed){
      if(same_session)SendEvent(msg.sender,browse::Encode(browse::Kind::Deny,p.id,msg.sender));
      break;
    }
    if(!LoadChosen(p.name,tx_data)){
      PHOTO_LOG("GMM photo owner: fetch denied because approved image is not readable");
      SendEvent(msg.sender,browse::Encode(browse::Kind::Deny,p.id,msg.sender));
      // Keep owner consent; failure of one image does not expose other files.
      Note("Secilen fotograf acilamadi. Dosyayi Pictures klasorunde kontrol et.");
      break;
    }
    outgoing_id=approved_id;outgoing_peer=approved_actor;
    if(SendEvent(outgoing_peer,wire::EncodeOffer(outgoing_id,outgoing_peer,tx_data))){
      outgoing_deadline=now+45;tx_accepted=false;tx_index=0;snap.sending=true;
      snap.progress=0;snap.total=tx_data.size();Note("Onaylanan fotograf gonderiliyor...");
    }else{ResetSession();Note("Photon fotograf teklifi gonderilemedi.");}
    // Keep owner approval for more images within this same session.
    break;
   }
   default:break;
  }
 }
 void Incoming(const Message& msg){
  if(msg.data.compare(0,7,"GMMKCK1")==0){KickIncoming(msg);return;}
  if(msg.data.compare(0,7,"GMMIMG2")==0){IncomingBrowse(msg);return;}
  wire::Packet p;if(!wire::Decode(msg.data,p)||!IsPeer(msg.sender)||!local_actor||!room_id)return;
  const double now=Now();
  switch(p.kind){
   case wire::Kind::Offer: {
    if(p.recipient!=local_actor||incoming_offer.id||rx.Active())return;
    const bool requested=chat_bot_bridge::Read().developer_unlocked&&browse_id==p.id&&browse_actor==msg.sender&&browse_fetching;
    if(!requested&&!photo_transfer_bridge::Read().receiving)return;
    if(requested){
      Offer approved{p.id,msg.sender,p.recipient,p.total,(p.total+kChunkBytes-1)/kChunkBytes,p.digest};
      if(!rx.BeginTrusted(approved,local_actor,true)||!SendEvent(msg.sender,wire::EncodeAccept(p.id,msg.sender))){
        rx.Cancel();browse_fetching=false;snap.fetching=false;Note("Fotograf alma baglantisi kurulamadi.");return;
      }
      incoming_offer=approved;requested_sender=msg.sender;incoming_deadline=now+30;
      snap.incoming=false;snap.total=p.total;snap.progress=0;Note("Secilen fotograf aliniyor...");return;
    }
    incoming_offer={p.id,msg.sender,p.recipient,p.total,(p.total+kChunkBytes-1)/kChunkBytes,p.digest};
    incoming_deadline=now+45;requested_sender=msg.sender;
    snap.incoming=true;snap.incoming_from=std::to_string(msg.sender);
    for(const auto& peer:peers)if(peer.actor==msg.sender)snap.incoming_from=peer.display;
    snap.progress=0;snap.total=p.total;Note("Gelen fotoğraf teklifi: Kabul et / Reddet");break;
   }
   case wire::Kind::Accept: {
    if(!outgoing_id||p.id!=outgoing_id||msg.sender!=outgoing_peer||p.recipient!=local_actor)return;
    tx_accepted=true;outgoing_deadline=now+30;Note("Karşı cihaz onayladı. Gönderiliyor...");break;
   }
   case wire::Kind::Chunk: {
    if(!chat_bot_bridge::Read().developer_unlocked||!rx.Active()||(!browse_fetching&&!photo_transfer_bridge::Read().receiving)){rx.Cancel();return;}
    if(rx.Push(p.id,msg.sender,p.index,p.bytes)){snap.progress+=p.bytes.size();incoming_deadline=now+30;browse_deadline=now+900;}
    break;
   }
   case wire::Kind::Done: {
    if(!rx.Active()||msg.sender!=requested_sender||p.id!=incoming_offer.id)return;
    Bytes photo;const bool authorized=chat_bot_bridge::Read().developer_unlocked;
    const bool verified=authorized&&rx.Finish(photo);
    const bool valid=verified&&SaveReceived(photo,p.id);
    PHOTO_LOG("GMM photo receiver: done authorized=%d verified=%d saved=%d bytes=%zu",authorized,verified,valid,photo.size());
    Note(valid?"Fotoğraf alındı: "+last_saved_path:"Fotoğraf doğrulanamadı veya kaydedilemedi");
    incoming_offer={};requested_sender=0;incoming_deadline=0;
    snap.incoming=false;snap.incoming_from.clear();
    if(browse_fetching){
      browse_fetching=false;snap.fetching=false;batch_finished++;
      browse_deadline=now+900;
      PHOTO_LOG("GMM photo batch: completed=%zu queued=%zu",batch_finished,batch.size());
    }
    break;
   }
   case wire::Kind::Cancel: {
    if(msg.sender==outgoing_peer&&p.id==outgoing_id){ResetSession();Note("Gönderim karşı cihaz tarafından iptal edildi.");}
    else if(msg.sender==requested_sender&&p.id==incoming_offer.id){rx.Cancel();incoming_offer={};requested_sender=0;snap.incoming=false;Note("Gönderici işlemi iptal etti.");}
    break;
   }
   default: break;
  }
 }
 void HandleCommands(const bridge::Commands& c){
  const double now=Now();
  if(c.choose_actor>0){
   PHOTO_LOG("GMM photo UI: choose actor=%d found=%d",c.choose_actor,IsPeer(c.choose_actor));
  }
  if(c.choose_actor>0&&IsPeer(c.choose_actor)){
   if(pending_actor!=c.choose_actor&&browse_id){
    browse_id=0;browse_actor=0;browse_granted=browse_fetching=false;
    snap.browsing=snap.listing_ready=snap.fetching=false;snap.remote_files.clear();selected_remote_file.clear();selections.clear();batch.clear();
   }
   pending_actor=c.choose_actor;pending_file.clear();
  }
  // Sharing can only be enabled by the owner-local config file, never a GUI
  // shortcut or an incoming remote packet.
  if(c.stop_sharing&&(sharing_enabled||approved_id)){
   sharing_enabled=false;
   if(approved_id){
    if(IsPeer(approved_actor))SendEvent(approved_actor,browse::Encode(browse::Kind::Deny,approved_id,approved_actor));
    approved_id=0;approved_actor=0;approved_files.clear();browse_outbox.clear();sharing_with.clear();approval_deadline=0;
   }
   if(outgoing_id&&IsPeer(outgoing_peer)){
    SendEvent(outgoing_peer,wire::EncodeCancel(outgoing_id));
    ResetSession();
   }
   Note("Pictures paylasimi kapatildi.");
  }
  if(c.cancel_browse&&browse_id&&!browse_fetching){
   PHOTO_LOG("GMM photo UI: browse cancelled target=%d",browse_actor);
   SendEvent(browse_actor,browse::Encode(browse::Kind::Deny,browse_id,browse_actor));
   browse_id=0;browse_actor=0;browse_deadline=0;
   browse_granted=browse_fetching=false;
   snap.browsing=snap.listing_ready=snap.fetching=false;
   snap.remote_files.clear();selected_remote_file.clear();selections.clear();batch.clear();
   Note("Fotoğraf listesi isteği iptal edildi. Yeniden deneyebilirsin.");
  }
  if(c.request_browse){
   if(!chat_bot_bridge::Read().developer_unlocked){Note("Gelistirici sifresi olmadan dosya listesi istenemez.");}
   else {
   PHOTO_LOG("GMM photo UI: browse requested room=%d local=%d selected=%d peer=%d active_request=%d",!!room_id,local_actor,pending_actor,IsPeer(pending_actor),!!browse_id);
   if(!IsPeer(pending_actor))Note("Once ayni odadan hedef oyuncu sec.");
   else if(browse_id||outgoing_id||rx.Active())Note("Once onceki istegin bitmesini bekle.");
   else {
    std::random_device rng;uint64_t id=(uint64_t(rng())<<32)|uint64_t(rng());if(!id)id=1;
    if(SendEvent(pending_actor,browse::Encode(browse::Kind::Request,id,pending_actor))){
     browse_id=id;browse_actor=pending_actor;browse_deadline=now+900;browse_granted=browse_fetching=false;
     selections.clear();batch.clear();batch_finished=0;
     snap.browsing=true;snap.listing_ready=snap.fetching=false;snap.remote_files.clear();selected_remote_file.clear();
     Note("Fotograf listesi icin diger oyuncunun onayi bekleniyor...");
    }else Note("Photon istek gonderilemedi. Ag baglantisini kontrol et.");
   }
   }
  }
  if(!c.choose_remote_file.empty()){
   const bool listed=std::find(snap.remote_files.begin(),snap.remote_files.end(),c.choose_remote_file)!=snap.remote_files.end();
   if(browse_granted&&snap.listing_ready&&listed&&!browse_fetching&&batch.empty()){
    auto it=std::find(selections.begin(),selections.end(),c.choose_remote_file);
    if(it==selections.end())selections.push_back(c.choose_remote_file);
    else selections.erase(it);
    selected_remote_file=selections.empty()?std::string():selections.back();
    Note(std::to_string(selections.size())+" fotograf secildi.");
   }
  }
  if(c.select_all&&browse_granted&&snap.listing_ready&&!browse_fetching&&batch.empty()){
   selections=snap.remote_files;selected_remote_file=selections.empty()?std::string():selections.back();
  }
  if(c.clear_selection&&!browse_fetching&&batch.empty()){selections.clear();selected_remote_file.clear();}
  if(c.fetch){
   if(selections.empty()&&snap.listing_ready&&snap.remote_files.size()==1)selections={snap.remote_files.front()};
   PHOTO_LOG("GMM photo UI: fetch batch selected=%zu authorized=%d active=%d",selections.size(),browse_granted,browse_fetching);
   if(!chat_bot_bridge::Read().developer_unlocked)Note("Gelistirici sifresi olmadan fotograf alinamaz.");
   else if(!browse_id||!browse_granted||!snap.listing_ready||selections.empty()||browse_fetching||!batch.empty()||!IsPeer(browse_actor))
    Note("Fotograf alinamadi: paylasimi ve secimi kontrol et.");
   else{batch=std::deque<std::string>(selections.begin(),selections.end());batch_finished=0;
    Note(std::to_string(batch.size())+" fotograf sirayla alinacak.");}
  }
  if(!c.choose_file.empty()){
   const auto found=std::find(snap.files.begin(),snap.files.end(),c.choose_file);
   if(found!=snap.files.end())pending_file=c.choose_file;
  }
  if(c.refresh){
   // The public GMM Connect refresh button must refresh the room's players,
   // not merely re-scan the sender's local picture directory.
   DiscoverRoom();snap.peers=peers;
   if(peers.empty())Note(room_id?"Ayni Photon odasinda baska oyuncu bulunamadi.":"Photon odasina girildikten sonra tekrar dene.");
  }
  if(c.reject&&incoming_offer.id){SendEvent(requested_sender,wire::EncodeCancel(incoming_offer.id));rx.Cancel();incoming_offer={};requested_sender=0;snap.incoming=false;snap.incoming_from.clear();Note("Teklif reddedildi.");}
  if(c.accept&&incoming_offer.id){
   if(photo_transfer_bridge::Read().receiving&&rx.BeginTrusted(incoming_offer,local_actor,true)&&SendEvent(requested_sender,wire::EncodeAccept(incoming_offer.id,requested_sender))){
     snap.incoming=false;Note("Alıcı kabul etti. Fotoğraf bekleniyor...");incoming_deadline=Now()+90;
   }else{rx.Cancel();incoming_offer={};requested_sender=0;snap.incoming=false;Note("Alıcı izni kapalı veya ağ yanıtı gönderilemedi.");}
  }
  if(c.offer){
   if(!outgoing_id&&IsPeer(pending_actor)&&!pending_file.empty()&&LoadChosen(pending_file,tx_data)){
    std::random_device rng;outgoing_id=(uint64_t(rng())<<32)|uint64_t(rng());if(!outgoing_id)outgoing_id=1;
    outgoing_peer=pending_actor;const auto offer=wire::EncodeOffer(outgoing_id,outgoing_peer,tx_data);
    if(SendEvent(outgoing_peer,offer)){outgoing_deadline=Now()+45;tx_accepted=false;tx_index=0;snap.sending=true;snap.total=tx_data.size();snap.progress=0;Note("Fotoğraf teklifi gönderildi. Karşı cihazın onayı bekleniyor.");}
    else{ResetSession();Note("Photon fotoğraf teklifi gönderilemedi.");}
   }else Note("Once oyuncu ve Pictures icinden JPG/PNG sec.");
  }
 }
 void TransferStep(){
  const double now=Now();
  if(approved_id&&(now>approval_deadline||!IsPeer(approved_actor))){approved_id=0;approved_actor=0;approved_files.clear();browse_outbox.clear();sharing_with.clear();}
  if(browse_id&&(now>browse_deadline||!IsPeer(browse_actor))){browse_id=0;browse_actor=0;browse_granted=browse_fetching=false;batch.clear();selections.clear();
   snap.browsing=snap.fetching=snap.listing_ready=false;snap.remote_files.clear();selected_remote_file.clear();Note("Fotograf isteginde zaman asimi.");}
  if(!batch.empty()&&!browse_fetching&&!rx.Active()&&browse_id&&browse_granted&&snap.listing_ready&&IsPeer(browse_actor)){
   const auto name=batch.front();
   if(SendEvent(browse_actor,browse::Encode(browse::Kind::Fetch,browse_id,browse_actor,name))){
    batch.pop_front();browse_fetching=true;snap.fetching=true;browse_deadline=now+900;
    Note("Fotograf isteniyor ("+std::to_string(batch_finished+1)+"): "+name);
   }else Note("Fotograf istegi gonderilemedi; tekrar deneniyor.");
  }
  if(!browse_outbox.empty()&&now>=next_browse_send){
   const auto& next=browse_outbox.front();
   if(SendEvent(next.first,next.second))browse_outbox.pop_front();
   next_browse_send=now+.08;
  }
  if(incoming_offer.id&&now>incoming_deadline){rx.Cancel();incoming_offer={};requested_sender=0;snap.incoming=false;Note("Gelen fotoğraf isteği zaman aşımına uğradı.");}
  if(outgoing_id&&now>outgoing_deadline){ResetSession();Note("Fotoğraf aktarımında zaman aşımı.");}
  if(!outgoing_id||!tx_accepted||now<next_send||!IsPeer(outgoing_peer))return;
  next_send=now+.036; // pace reliable packets for slower emulators; one image at a time
  const size_t chunks=(tx_data.size()+kChunkBytes-1)/kChunkBytes;
  if(tx_index<chunks){const auto data=wire::EncodeChunk(outgoing_id,tx_index,tx_data);
   if(SendEvent(outgoing_peer,data)){
    snap.progress=std::min(tx_data.size(),(tx_index+1)*kChunkBytes);++tx_index;outgoing_deadline=now+30;
   }else Note("Fotoğraf paketi gönderilemedi; bağlantı tekrar deneniyor.");
  }else{
   if(SendEvent(outgoing_peer,wire::EncodeDone(outgoing_id))){
    ResetSession();Note("Tüm fotoğraf paketleri gönderildi. Alıcı kaydı ayrıca doğrulamalı.");
   }
  }
 }
public:
 explicit Session(const Api& api):a(api){}
 void Install(){valid=Resolve();snap.ready=valid;Note(valid?"Photon GMM fotoğraf kanalı hazır":"Photon sürümü uyumsuz: ağ bağlantısı kurulamadı");PHOTO_LOG("GMM photo transport hook=%d",valid);bridge::Publish(snap);}
 void Pump(bool loading){
  if(!valid)return;
  if(loading){ResetSession();ResetBrowse();bridge::Publish(snap);return;}
  if(Now()>next_poll){next_poll=Now()+1.0;DiscoverRoom();snap.peers=peers;}
  if(Now()>next_policy_check){
   next_policy_check=Now()+2.0;
   LoadAdminKey();
   const bool owner_enabled=SharingPolicyEnabled();
   if(sharing_enabled&&!owner_enabled){
    if(approved_id&&IsPeer(approved_actor))SendEvent(approved_actor,browse::Encode(browse::Kind::Deny,approved_id,approved_actor));
    approved_id=0;approved_actor=0;approval_deadline=0;approved_files.clear();browse_outbox.clear();sharing_with.clear();
   }
   sharing_enabled=owner_enabled;
  }
  snap.in_room=!!room_id;
  snap.local_actor=local_actor;
  if(Now()>next_diagnostic){
   next_diagnostic=Now()+8.0;
   PHOTO_LOG("GMM photo runtime: room=%d local_actor=%d peers=%zu selected=%d browse=%d room_sharing=%d",!!room_id,local_actor,peers.size(),pending_actor,!!browse_id,sharing_enabled);
  }
  if(!room_id){ResetSession();ResetBrowse();snap.peers.clear();snap.ready=valid;
   kick_snapshot.in_room=false;kick_snapshot.selected_actor=0;kick_snapshot.peers.clear();kick_bridge::Publish(kick_snapshot);
   bridge::Publish(snap);return;}
  std::deque<Message> pending;{std::lock_guard<std::mutex> l(inbox_mutex);pending.swap(inbox);}
  for(const auto& msg:pending)Incoming(msg);
  HandleCommands(bridge::Take());
  HandleKickCommands();
  // Locking the developer session immediately revokes requesting/receiving.
  // Consent to SEND is independent: a locked user can still approve sharing.
  if(!chat_bot_bridge::Read().developer_unlocked&&browse_id){
   if(IsPeer(browse_actor))SendEvent(browse_actor,browse::Encode(browse::Kind::Deny,browse_id,browse_actor));
   browse_id=0;browse_actor=0;browse_deadline=0;
   browse_granted=browse_fetching=false;snap.browsing=snap.fetching=snap.listing_ready=false;
   snap.remote_files.clear();selected_remote_file.clear();selections.clear();batch.clear();
   rx.Cancel();incoming_offer={};requested_sender=0;snap.incoming=false;
   Note("Gelistirici kilidi kapandi; fotograf alma iptal edildi.");
  }
  if(!browse_fetching&&!photo_transfer_bridge::Read().receiving&&rx.Active()){rx.Cancel();incoming_offer={};requested_sender=0;snap.incoming=false;Note("Alma modu kapatıldı; transfer durduruldu.");}
  TransferStep();
  snap.selected_actor=pending_actor;snap.selected_file=pending_file;snap.selected_remote_file=selected_remote_file;
  snap.selected_remote_files=selections;snap.queue_remaining=batch.size()+(browse_fetching?1U:0U);
  snap.share_active=sharing_enabled;
  snap.selected_name.clear();for(const auto& peer:peers)if(peer.actor==pending_actor)snap.selected_name=peer.display;
  bridge::Publish(snap);
 }
};
Session* controller=nullptr;
}
void Install(const il2cpp::Api& api){static bool initialized=false;if(initialized)return;initialized=true;
 // All metadata discovery and hooks execute on Unity's already-attached thread.
 static Session runtime(api);controller=&runtime;runtime.Install();
}
void Pump(const il2cpp::Api&,bool loading){if(controller)controller->Pump(loading);}
}
#endif
