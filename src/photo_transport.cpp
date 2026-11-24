#include "photo_transport.h"
#ifdef __ANDROID__
#include "photo_transfer_bridge.h"
#include "storage_access.h"
#include "photo_transport_bridge.h"
#include "photo_transfer_protocol.h"
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
const Method *event_indexer=nullptr,*event_data=nullptr;
struct Message {int sender=0;std::string data;};
std::mutex inbox_mutex;std::deque<Message> inbox;
void EventHook(Object* client,Object* event,const Method* method) {
 // Read EventData *before* forwarding. Photon may recycle/clear event payloads
 // after callbacks. Only copy a bounded string; process state on Unity Update.
 try {
  if(event_api&&event&&event_code_field&&event_indexer&&event_data) {
   const Api& a=*event_api;
   uint8_t code=0;
   a.field_get_value(event,event_code_field,&code);
   if(code==wire::kEventCode){
    bool ok=false;
    uint8_t sender_key=254;void* args[]={&sender_key};
    Root sender(a,a.Call(event_indexer,event,args,&ok));
    if(ok&&sender.Get()&&a.object_get_class) {
     auto* sender_type=a.object_get_class(sender.Get());
     const bool is_int=sender_type&&!std::strcmp(a.class_get_name(sender_type),"Int32")&&
                              !std::strcmp(a.class_get_namespace(sender_type),"System");
     const int actor=is_int?*static_cast<int*>(a.object_unbox(sender.Get())):0;
     if(actor>0){
      Root data(a,a.Call(event_data,event,nullptr,&ok));
      if(ok&&data.Get()&&a.object_get_class){
       auto* klass=a.object_get_class(data.Get());
       if(klass&&!std::strcmp(a.class_get_name(klass),"String")&&
          !std::strcmp(a.class_get_namespace(klass),"System")){
        std::string line=a.Utf8(reinterpret_cast<String*>(data.Get()));
        if(line.size()<=1800&&line.compare(0,7,"GMMIMG1")==0){
         std::lock_guard<std::mutex> lock(inbox_mutex);
         if(inbox.size()<64)inbox.push_back({actor,std::move(line)});
        }
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
static const std::string kShareDir="/sdcard/Pictures/GMMShare";
class Session {
 const Api& a;
 const Method *raise{},*current_room{},*local_player{},*players{},*actor_get{},*nick_get{},*array_length{},*array_item{};
 Class *opt_class{},*send_class{},*int_class{};
 Field *target_actors{},*reliable{},*encrypted{};
 const Method* send_reliable_get{};
 const size_t max_send_size=32;
 bool valid=false,installed=false;
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
 std::string pending_file;
 std::vector<bridge::Peer> peers;
 std::string private_files_dir;
 std::string last_saved_path;

 const Method* M(const char* ns,const char* cls,const char* name,int n=0){return a.FindMethod(a.Find(ns,cls),name,n);}
 bool IsPeer(int actor)const {return actor>0&&std::any_of(peers.begin(),peers.end(),[&](const auto& p){return p.actor==actor;});}
 void Note(std::string s){snap.status=std::move(s);}
 void ResetSession(){
  rx.Cancel();incoming_offer={};requested_sender=0;outgoing_id=0;outgoing_peer=0;tx_data.clear();tx_index=0;tx_accepted=false;
  incoming_deadline=outgoing_deadline=0;snap.incoming=snap.sending=false;snap.incoming_from.clear();snap.progress=snap.total=0;
 }
 bool Resolve() {
  if(!a.array_new||!a.array_length||!a.array_element_size||!a.runtime_class_init||!a.array_object_header_size||!a.class_from_type||!a.class_get_field_from_name||!a.field_set_value_object||!a.object_get_class||!a.object_new||!a.field_static_get_value||!a.field_get_value||!a.field_set_value||!a.class_value_size)return false;
  raise=M("Photon.Pun","PhotonNetwork","RaiseEvent",4);
  current_room=M("Photon.Pun","PhotonNetwork","get_CurrentRoom");
  local_player=M("Photon.Pun","PhotonNetwork","get_LocalPlayer");
  players=M("Photon.Pun","PhotonNetwork","get_PlayerListOthers");
  actor_get=M("Photon.Realtime","Player","get_ActorNumber");
  nick_get=M("Photon.Realtime","Player","get_NickName");
  array_length=M("System","Array","get_Length");
  array_item=M("System","Array","GetValueImpl",1);
  opt_class=a.Find("Photon.Realtime","RaiseEventOptions");
  send_class=a.Find("ExitGames.Client.Photon","SendOptions");
  int_class=a.Find("System","Int32");
  target_actors=opt_class?a.class_get_field_from_name(opt_class,"TargetActors"):nullptr;
  reliable=send_class?a.class_get_field_from_name(send_class,"SendReliable"):nullptr;
  encrypted=send_class?a.class_get_field_from_name(send_class,"Encrypt"):nullptr;
  send_reliable_get=a.FindMethod(send_class,"get_SendReliable",0);
  auto* ev=a.Find("ExitGames.Client.Photon","EventData");
  event_code_field=ev?a.class_get_field_from_name(ev,"Code"):nullptr;
  event_indexer=a.FindMethod(ev,"get_Item",1);
  event_data=a.FindMethod(ev,"get_CustomData",0);
  auto* client=a.Find("Photon.Realtime","LoadBalancingClient");
  auto* on_event=a.FindMethod(client,"OnEvent",1);
  if(!raise||!current_room||!local_player||!players||!actor_get||!nick_get||!array_length||!array_item||
     !opt_class||!send_class||!int_class||!target_actors||!encrypted||(!reliable&&!send_reliable_get)||
     !event_code_field||!event_indexer||!event_data||!on_event||!Entry(on_event))return false;
  uint32_t alignment=0;const int send_size=a.class_value_size(send_class,&alignment);
  if(send_size<=0||send_size>int(max_send_size)||alignment>alignof(std::max_align_t))return false;
  a.runtime_class_init(send_class);
  event_api=&a;
  installed=DobbyHook(Entry(on_event),reinterpret_cast<void*>(EventHook),reinterpret_cast<void**>(&original_event))==0 && original_event;
  return installed;
 }
 bool SendEvent(int recipient,const std::string& text){
  if(!valid||!IsPeer(recipient)||text.empty()||text.size()>1800)return false;
  Root option(a,a.object_new(opt_class));Root target(a,a.array_new(int_class,1));
  if(!option.Get()||!target.Get()||a.array_length(target.Get())!=1)return false;
  auto* target_class=a.object_get_class(target.Get());
  if(!target_class||a.array_element_size(target_class)!=4)return false;
  const size_t header=a.array_object_header_size();
  if(header<24||header>128||header%4)return false;
  std::memcpy(reinterpret_cast<uint8_t*>(target.Get())+header,&recipient,sizeof(recipient));
  a.field_set_value_object(option.Get(),target_actors,target.Get());
  Root settings(a,a.object_new(send_class));if(!settings.Get())return false;
  uint32_t align=0;const int n=a.class_value_size(send_class,&align);
  if(n<=0||n>int(max_send_size))return false;
  // Use Photon SDK's SendReliable value and set Encrypt=true. Photon encryption
  // is client<->server, NOT end-to-end encryption between the two phones.
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
  bool ok=false;Root sent(a,a.Call(raise,nullptr,args,&ok));
  return ok&&sent.Get()&&*static_cast<bool*>(a.object_unbox(sent.Get()));
 }
 void DiscoverRoom(){
  Root room(a,a.Call(current_room));const uintptr_t id=reinterpret_cast<uintptr_t>(room.Get());
  if(room_id&&room_id!=id){
   ResetSession();pending_actor=0;pending_file.clear();
   std::lock_guard<std::mutex> lock(inbox_mutex);inbox.clear();
  }
  room_id=id;
  peers.clear();local_actor=0;
  if(!room.Get())return;
  Root me(a,a.Call(local_player));
  local_actor=a.Value<int>(actor_get,me.Get(),0);
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
  mkdir("/sdcard/Pictures",0755);mkdir(kShareDir.c_str(),0700);
  DIR* dir=opendir(kShareDir.c_str());if(!dir)return files;
  for(dirent* entry;(entry=readdir(dir))!=nullptr && files.size()<40;){
   const std::string name=entry->d_name;
   if(name.empty()||name[0]=='.'||name.find('/')!=std::string::npos||name.find('\\')!=std::string::npos||name.size()>128)continue;
   const auto dot=name.find_last_of('.');if(dot==std::string::npos)continue;
   std::string ext=name.substr(dot);std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
   if(ext!=".jpg"&&ext!=".jpeg"&&ext!=".png")continue;
   struct stat st{};const std::string full=kShareDir+"/"+name;
   if(lstat(full.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_size>=8&&st.st_size<=static_cast<off_t>(kMaxImageBytes))files.push_back(name);
  }
  closedir(dir);std::sort(files.begin(),files.end());return files;
 }
 bool LoadChosen(std::string filename,Bytes& bytes){
  if(filename.empty()||filename.size()>128||filename=="."||filename==".."||filename.find('/')!=std::string::npos||filename.find('\\')!=std::string::npos)return false;
  const auto path=kShareDir+"/"+filename;
  struct stat st{};if(lstat(path.c_str(),&st)!=0||!S_ISREG(st.st_mode)||st.st_size<8||st.st_size>static_cast<off_t>(kMaxImageBytes))return false;
  std::ifstream in(path,std::ios::binary);if(!in)return false;
  bytes.resize(static_cast<size_t>(st.st_size));
  in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
  return in.gcount()==static_cast<std::streamsize>(bytes.size())&&IsImage(bytes);
 }
 bool SaveReceived(const Bytes& bytes,uint64_t id){
  last_saved_path.clear();
  if(!IsImage(bytes))return false;
  std::string dest;
  if(storage_access::Granted()) {
   // User enabled Android's broad storage access. Only write a received and
   // expressly accepted image; never enumerate or export other device files.
   mkdir("/sdcard/Pictures",0755);
   dest="/sdcard/Pictures/GMMReceived";
  } else {
   if(private_files_dir.empty()){
    std::ifstream cmd("/proc/self/cmdline",std::ios::binary);std::string pkg;std::getline(cmd,pkg,'\0');
    if(pkg.empty()||pkg.find('/')!=std::string::npos)return false;
    const std::string base="/data/user/0/"+pkg+"/files";
    struct stat st{};if(stat(base.c_str(),&st)!=0||!S_ISDIR(st.st_mode))return false;
    private_files_dir=base;
   }
   dest=private_files_dir+"/GMMReceived";
  }
  if(mkdir(dest.c_str(),0700)!=0&&errno!=EEXIST)return false;
  struct stat dir_stat{};
  if(lstat(dest.c_str(),&dir_stat)!=0||!S_ISDIR(dir_stat.st_mode))return false;
  const bool png=bytes.size()>=8&&bytes[0]==137&&bytes[1]==80;
  const std::string path=dest+"/received_"+std::to_string(id)+(png?".png":".jpg");
  // Exclusive create prevents overwriting any existing user file and avoids
  // following a malicious symlink with the same destination name.
  const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
  if(fd<0)return false;
  size_t written=0;bool ok=true;
  while(written<bytes.size()){
   const ssize_t n=write(fd,bytes.data()+written,bytes.size()-written);
   if(n<=0){if(n<0&&errno==EINTR)continue;ok=false;break;}
   written+=size_t(n);
  }
  if(close(fd)!=0)ok=false;
  if(!ok){unlink(path.c_str());return false;}
  last_saved_path=path;return true;
 }

 void Incoming(const Message& msg){
  wire::Packet p;if(!wire::Decode(msg.data,p)||!IsPeer(msg.sender)||!local_actor||!room_id)return;
  const double now=Now();
  switch(p.kind){
   case wire::Kind::Offer: {
    if(p.recipient!=local_actor||!photo_transfer_bridge::Read().receiving||incoming_offer.id||rx.Active())return;
    incoming_offer={p.id,msg.sender,p.recipient,p.total,(p.total+kChunkBytes-1)/kChunkBytes,p.digest};
    incoming_deadline=now+45;requested_sender=msg.sender;
    snap.incoming=true;snap.incoming_from=std::to_string(msg.sender);
    for(const auto& peer:peers)if(peer.actor==msg.sender)snap.incoming_from=peer.display;
    snap.progress=0;snap.total=p.total;Note("Gelen fotoğraf teklifi: Kabul et / Reddet");break;
   }
   case wire::Kind::Accept: {
    if(!outgoing_id||p.id!=outgoing_id||msg.sender!=outgoing_peer||p.recipient!=local_actor)return;
    tx_accepted=true;outgoing_deadline=now+90;Note("Karşı cihaz onayladı. Gönderiliyor...");break;
   }
   case wire::Kind::Chunk: {
    if(!rx.Active()||!photo_transfer_bridge::Read().receiving){rx.Cancel();return;}
    if(rx.Push(p.id,msg.sender,p.index,p.bytes))snap.progress+=p.bytes.size();
    break;
   }
   case wire::Kind::Done: {
    if(!rx.Active()||msg.sender!=requested_sender||p.id!=incoming_offer.id)return;
    Bytes photo;const bool valid=rx.Finish(photo)&&SaveReceived(photo,p.id);
    Note(valid?"Fotoğraf alındı: "+last_saved_path:"Fotoğraf doğrulanamadı veya kaydedilemedi");
    incoming_offer={};requested_sender=0;incoming_deadline=0;
    snap.incoming=false;snap.incoming_from.clear();break;
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
  if(c.choose_actor>0&&IsPeer(c.choose_actor)){pending_actor=c.choose_actor;pending_file.clear();}
  if(!c.choose_file.empty()){
   const auto found=std::find(snap.files.begin(),snap.files.end(),c.choose_file);
   if(found!=snap.files.end())pending_file=c.choose_file;
  }
  if(c.refresh) snap.files=LocalImages();
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
   }else Note("Önce oyuncu ve GMMShare içinden 256 KB altında JPG/PNG seç.");
  }
 }
 void TransferStep(){
  const double now=Now();
  if(incoming_offer.id&&now>incoming_deadline){rx.Cancel();incoming_offer={};requested_sender=0;snap.incoming=false;Note("Gelen fotoğraf isteği zaman aşımına uğradı.");}
  if(outgoing_id&&now>outgoing_deadline){ResetSession();Note("Fotoğraf aktarımında zaman aşımı.");}
  if(!outgoing_id||!tx_accepted||now<next_send||!IsPeer(outgoing_peer))return;
  next_send=now+.07;
  const size_t chunks=(tx_data.size()+kChunkBytes-1)/kChunkBytes;
  if(tx_index<chunks){const auto data=wire::EncodeChunk(outgoing_id,tx_index,tx_data);
   if(SendEvent(outgoing_peer,data)){
    snap.progress=std::min(tx_data.size(),(tx_index+1)*kChunkBytes);++tx_index;
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
  if(loading){ResetSession();bridge::Publish(snap);return;}
  if(Now()>next_poll){next_poll=Now()+1.0;DiscoverRoom();snap.peers=peers;}
  if(!room_id){ResetSession();snap.peers.clear();snap.ready=valid;bridge::Publish(snap);return;}
  std::deque<Message> pending;{std::lock_guard<std::mutex> l(inbox_mutex);pending.swap(inbox);}
  for(const auto& msg:pending)Incoming(msg);
  HandleCommands(bridge::Take());
  if(!photo_transfer_bridge::Read().receiving&&rx.Active()){rx.Cancel();incoming_offer={};requested_sender=0;snap.incoming=false;Note("Alma modu kapatıldı; transfer durduruldu.");}
  TransferStep();
  snap.selected_actor=pending_actor;snap.selected_file=pending_file;
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
