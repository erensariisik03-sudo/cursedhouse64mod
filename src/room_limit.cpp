#include "room_limit.h"
#ifdef __ANDROID__
#include "room_limit_bridge.h"
#include "chat_bot_bridge.h"
#include <android/log.h>
#include <chrono>
#include <cstring>
namespace room_limit { namespace {
using namespace il2cpp;
namespace b=room_limit_bridge;
using Clock=std::chrono::steady_clock;
struct Root {const Api& a; Handle h=0; Root(const Api& api,Object* p):a(api){if(p)h=a.gchandle_new(p,false);}~Root(){if(h)a.gchandle_free(h);}Object* Get()const{return h?a.gchandle_get_target(h):nullptr;}Root(const Root&)=delete;Root& operator=(const Root&)=delete;};
struct Methods {const Method *room{},*in_room{},*local{},*master{},*actor{},*limit{},*set_limit{},*count{};bool ready=false;};
Methods m;
b::Snapshot s;
Handle room_handle=0;
Clock::time_point next_poll{},resolve_retry{},verify_at{};
int requested=0;
const Method* Find(const Api& a,const char* ns,const char* cls,const char* name,int count=0){return a.FindMethod(a.Find(ns,cls),name,count);}
bool TypeIs(const Api& a,const Type* t,const char* name){if(!t||!a.class_from_type||!a.class_get_name||!a.class_get_namespace)return false;auto* c=a.class_from_type(t);return c&&std::strcmp(a.class_get_namespace(c),"System")==0&&std::strcmp(a.class_get_name(c),name)==0;}
bool IntValue(const Api& a,const Method* method,Object* self,int& value){bool ok=false;Root boxed(a,a.Call(method,self,nullptr,&ok));auto* o=boxed.Get();if(!ok||!o||!a.object_get_class)return false;auto* c=a.object_get_class(o);if(!c||std::strcmp(a.class_get_name(c),"Int32")||std::strcmp(a.class_get_namespace(c),"System"))return false;auto* p=a.object_unbox(o);if(!p)return false;value=*static_cast<int*>(p);return true;}
void ClearRoom(const Api& a){if(room_handle){a.gchandle_free(room_handle);room_handle=0;++s.generation;}s.in_room=s.is_host=s.busy=false;s.local_actor=s.master_actor=s.count=s.limit=0;requested=0;}
void Resolve(const Api& a){m.room=Find(a,"Photon.Pun","PhotonNetwork","get_CurrentRoom");m.in_room=Find(a,"Photon.Pun","PhotonNetwork","get_InRoom");m.local=Find(a,"Photon.Pun","PhotonNetwork","get_LocalPlayer");m.master=Find(a,"Photon.Pun","PhotonNetwork","get_MasterClient");m.actor=Find(a,"Photon.Realtime","Player","get_ActorNumber");m.limit=Find(a,"Photon.Realtime","Room","get_MaxPlayers");m.set_limit=Find(a,"Photon.Realtime","Room","set_MaxPlayers",1);m.count=Find(a,"Photon.Realtime","Room","get_PlayerCount");m.ready=m.room&&m.in_room&&m.local&&m.master&&m.actor&&m.limit&&m.set_limit&&m.count&&a.method_get_param&&TypeIs(a,a.method_get_param(m.set_limit,0),"Int32");__android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM room limit stable-r1: resolved=%d",m.ready);}
} // namespace
void Pump(const Api& a,bool loading){
 const auto now=Clock::now();const auto cmd=b::Take();
 if(loading){ClearRoom(a);s.status="Sahne yukleniyor.";b::Publish(s);return;}
 if(!m.ready&&now>=resolve_retry){Resolve(a);resolve_retry=now+std::chrono::seconds(5);}
 s.ready=m.ready;
 if(!m.ready){s.status="Photon oda API uyumsuz.";b::Publish(s);return;}
 if(!cmd.limit&&now<next_poll)return;
 next_poll=now+std::chrono::milliseconds(500);
 if(!a.Value<bool>(m.in_room,nullptr,false)){ClearRoom(a);s.status="Bir odaya gir.";b::Publish(s);return;}
 Root room(a,a.Call(m.room));if(!room.Get()){ClearRoom(a);s.status="Oda bulunamadi.";b::Publish(s);return;}
 if(!room_handle||a.gchandle_get_target(room_handle)!=room.Get()){ClearRoom(a);room_handle=a.gchandle_new(room.Get(),false);++s.generation;s.status="Oda bilgisi okundu.";}
 if(!room_handle){ClearRoom(a);s.status="Oda referansi alinamadi.";b::Publish(s);return;}
 Root local(a,a.Call(m.local)),master(a,a.Call(m.master));
 s.in_room=true;s.local_actor=s.master_actor=s.count=s.limit=0;
 bool actors=local.Get()&&master.Get()&&IntValue(a,m.actor,local.Get(),s.local_actor)&&IntValue(a,m.actor,master.Get(),s.master_actor);
 bool values=IntValue(a,m.count,room.Get(),s.count)&&IntValue(a,m.limit,room.Get(),s.limit);
 s.is_host=actors&&s.local_actor>0&&s.local_actor==s.master_actor;
 s.ready=m.ready&&values&&actors;
 if(s.busy&&now>=verify_at){s.busy=false;s.status=(values&&s.limit==requested)?"Limit yerel oda bilgisinde dogrulandi; diger cihazdan kontrol et.":"Limit dogrulanamadi.";__android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM room limit: requested=%d observed=%d match=%d",requested,s.limit,values&&s.limit==requested);requested=0;}
 if(cmd.limit){
  if(!b::Allowed(s,cmd,chat_bot_bridge::Read().developer_unlocked))s.status="Istek reddedildi: oda/yetki veya oyuncu sayisi degisti.";
  else if(s.limit==cmd.limit)s.status="Limit zaten bu degerde.";
  else {int value=cmd.limit;void* args[]={&value};bool ok=false;a.Call(m.set_limit,room.Get(),args,&ok);__android_log_print(ANDROID_LOG_INFO,"multiplayermod","GMM room limit: request=%d local=%d master=%d invocation=%d",value,s.local_actor,s.master_actor,ok);if(ok){requested=value;s.busy=true;verify_at=now+std::chrono::seconds(3);s.status="Istek gonderildi; oda bilgisi kontrol ediliyor.";}else s.status="Photon cagrisi basarisiz.";}
 }
 b::Publish(s);
}
}
#endif
