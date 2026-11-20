#include "cosmetic_room.h"
#include "wardrobe_bridge.h"
#include <android/log.h>
#include <chrono>
#include <vector>
#include <algorithm>

#define ROOM_LOG(...) __android_log_print(ANDROID_LOG_INFO,"multiplayermod",__VA_ARGS__)
namespace cosmetic_room {
namespace {
using namespace il2cpp;
struct Root {
    const Api& api; Handle h=0;
    Root(const Api& a,Object* value=nullptr):api(a),h(value?a.gchandle_new(value,false):0){}
    ~Root(){if(h)api.gchandle_free(h);}
    Root(const Root&)=delete;
    Root& operator=(const Root&)=delete;
    Object* Get()const{return h?api.gchandle_get_target(h):nullptr;}
    void Set(Object* value){Handle next=value?api.gchandle_new(value,false):0;if(h)api.gchandle_free(h);h=next;}
};
struct Runtime {
    const Api& api;
    Plan plan;
    Root locker,manager,key_accessory,key_skin;
    Class *locker_class{},*manager_class{},*string_class{};
    const Method *find{},*array_length{},*array_item{},*active{},*alive{},*uid{},*connected{},*player{},*room{},*properties{};
    // Wardrobe selection is the source of truth. The existing local equipped
    // profile is mirrored only after a wardrobe choice has been observed.
    const Method *profile_get{},*equipped_get{};
    Class* equipped_class{};
    std::array<const Method*,2> equipped_read{},equipped_write{};
    std::array<const Method*,2> selection{},wardrobe_select{},publish{},key_get{};
    std::array<std::optional<std::string>,2> wardrobe_values{};
    std::array<Field*,2> dirty{};
    bool initialized=false,disabled=false,started=false,busy=false;
    bool owner_known=false;
    std::string owner;
    std::array<bool,2> wait_clean{};
    std::array<bool,2> reporting{};
    std::array<bool,2> refresh{};
    const Method *leave_room{},*join_room{},*in_room{},*room_name{},*room_count{},*room_open{};
    int last_applied=0;
    int rejoin=0; std::string rejoin_name; double rejoin_deadline=0,rejoin_next=0;
    Class *loader_class{},*lobby_class{},*view_class{};
    const Method *loader_load{},*is_mine{},*actor_number{};
    double start=0,next_discovery=0,next_poll=0;
    explicit Runtime(const Api& a):api(a),locker(a),manager(a),key_accessory(a),key_skin(a){}
    Object* Call(const Method* method,Object* self=nullptr){bool ok=false;auto* r=api.Call(method,self,nullptr,&ok);return ok?r:nullptr;}
    bool Bool(const Method* method,Object* self=nullptr){bool ok=false;auto* r=api.Call(method,self,nullptr,&ok);return ok&&r&&*static_cast<bool*>(api.object_unbox(r));}
    Object* Ref(Object* self,const char* name){
        if(!self)return nullptr;
        auto* f=api.class_get_field_from_name(api.object_get_class(self),name);Object* r=nullptr;
        if(f)api.field_get_value(self,f,&r);
        return r;
    }
    bool IsAlive(Object* self){if(!self)return false;void* args[]={self};bool ok=false;auto* r=api.Call(alive,nullptr,args,&ok);return ok&&r&&*static_cast<bool*>(api.object_unbox(r));}
    std::optional<std::string> Text(Object* text){
        if(!text)return std::string{};
        if(api.object_get_class(text)!=string_class)return {};
        auto* s=reinterpret_cast<String*>(text);const int n=api.string_length(s);
        if(n<0||n>256)return {};
        const auto* chars=api.string_chars(s);if(!chars&&n)return {};
        for(int i=0;i<n;++i)if(!chars[i])return {};
        return api.Utf8(s);
    }
    bool Resolve(){
        if(!api.field_get_value||!api.class_get_field_from_name||!api.object_get_class||!api.class_get_type||!api.type_get_object)return false;
        locker_class=api.Find("","LockerManager");manager_class=api.Find("","PhotonManager");string_class=api.Find("System","String");
        auto* network=api.Find("Photon.Pun","PhotonNetwork");
        find=api.FindMethod(api.Find("UnityEngine","Object"),"FindObjectsByType",3);
        auto* array=api.Find("System","Array");array_length=api.FindMethod(array,"get_Length",0);array_item=api.FindMethod(array,"GetValueImpl",1);
        active=api.FindMethod(api.Find("UnityEngine","Behaviour"),"get_isActiveAndEnabled",0);
        alive=api.FindMethod(api.Find("UnityEngine","Object"),"op_Implicit",1);
        uid=api.FindMethod(api.Find("","fi"),"cvb",0);
        profile_get=api.FindMethod(api.Find("","fi"),"cvd",0);
        equipped_get=api.FindMethod(api.Find("","et"),"cqj",0);
        equipped_class=api.Find("","ew");
        equipped_read={api.FindMethod(equipped_class,"crj",0),api.FindMethod(equipped_class,"crh",0)};
        equipped_write={api.FindMethod(equipped_class,"crk",1),api.FindMethod(equipped_class,"cri",1)};
        loader_class=api.Find("","PlayerCosmeticsLoader");lobby_class=api.Find("","PlayerLobbyScript");view_class=api.Find("Photon.Pun","PhotonView");
        loader_load=api.FindMethod(loader_class,"Load",2);is_mine=api.FindMethod(view_class,"get_IsMine",0);
        actor_number=api.FindMethod(api.Find("Photon.Realtime","Player"),"get_ActorNumber",0);
        in_room=api.FindMethod(network,"get_InRoom",0);leave_room=api.FindMethod(network,"LeaveRoom",1);join_room=api.FindMethod(network,"JoinRoom",2);
        {auto* rc=api.Find("Photon.Realtime","Room");room_name=api.FindMethod(rc,"get_Name",0);room_count=api.FindMethod(rc,"get_PlayerCount",0);room_open=api.FindMethod(rc,"get_IsOpen",0);}
        connected=api.FindMethod(network,"get_IsConnectedAndReady",0);
        player=api.FindMethod(network,"get_LocalPlayer",0);room=api.FindMethod(network,"get_CurrentRoom",0);
        properties=api.FindMethod(api.Find("Photon.Realtime","Player"),"get_CustomProperties",0);
        selection={api.FindMethod(locker_class,"flh",0),api.FindMethod(locker_class,"flj",0)};
        // Use the game's own wardrobe click handlers (normal ownership/validation
        // path) instead of writing the account's equipped-profile fields directly.
        wardrobe_select={api.FindMethod(locker_class,"flm",1),api.FindMethod(locker_class,"fln",1)};
        publish={api.FindMethod(manager_class,"gna",1),api.FindMethod(manager_class,"gnb",1)};
        if(locker_class)dirty={api.class_get_field_from_name(locker_class,"pyc"),api.class_get_field_from_name(locker_class,"pyd")};
        // The publisher and receiver use these exact game decoder methods. Do
        // not invent protocol keys or copy native addresses into runtime code.
        auto* decoder=api.Find("<PrivateImplementationDetails>{B6FBF95C-D343-4B5E-9090-977A04841BC0}","a");
        key_get={api.FindMethod(decoder,"dms",0),api.FindMethod(decoder,"dmt",0)};
        // flm/fln are only needed when a request originates from our custom UI.
        // Their absence must not disable ordinary wardrobe observation/publication.
        const Method* required[]={find,array_length,array_item,active,alive,uid,connected,player,room,
            selection[0],selection[1],publish[0],publish[1],profile_get,equipped_get,
            equipped_read[0],equipped_read[1],equipped_write[0],equipped_write[1]};
        return string_class&&locker_class&&manager_class&&equipped_class&&dirty[0]&&dirty[1]&&
            std::all_of(std::begin(required),std::end(required),[](const Method* m){return m!=nullptr;});
    }
    Object* FindOne(Class* klass){
        const auto* type=api.class_get_type(klass);Root managed_type(api,type?api.type_get_object(type):nullptr);
        if(!managed_type.Get())return nullptr;
        int excludeInactive=0,noSorting=0;
        void* args[]={managed_type.Get(),&excludeInactive,&noSorting};bool ok=false;Root array(api,api.Call(find,nullptr,args,&ok));
        if(!ok||!array.Get())return nullptr;
        Root count(api,Call(array_length,array.Get()));if(!count.Get())return nullptr;
        const int n=*static_cast<int*>(api.object_unbox(count.Get()));if(n<0||n>16)return nullptr;
        Object* chosen=nullptr;
        for(int i=0;i<n;++i){
            void* index[]={&i};Object* value=api.Call(array_item,array.Get(),index,&ok);
            if(!ok)return nullptr;
            if(!IsAlive(value))continue;
            // Do not retain inactive UI/prefab objects and their referenced assets.
            if(!Bool(active,value))continue;
            if(chosen)return nullptr;
            chosen=value;
        }
        return chosen;
    }
    std::optional<bool> Matches(Object* local,size_t category,const std::string& desired){
        if(!properties)return {};
        Root& key=category==0?key_accessory:key_skin;
        if(!key.Get())key.Set(Call(key_get[category]));
        if(!key.Get()||!Text(key.Get()))return {};
        Root table(api,Call(properties,local));if(!table.Get())return {};
        auto* klass=api.object_get_class(table.Get());
        const auto* has=api.FindMethod(klass,"ContainsKey",1);const auto* get=api.FindMethod(klass,"get_Item",1);
        if(!has||!get)return {};
        void* args[]={key.Get()};bool ok=false;Root exists(api,api.Call(has,table.Get(),args,&ok));
        if(!ok||!exists.Get())return {};
        if(!*static_cast<bool*>(api.object_unbox(exists.Get())))return false;
        Root value(api,api.Call(get,table.Get(),args,&ok));if(!ok)return {};
        const auto value_text=Text(value.Get());return value_text?std::optional<bool>(*value_text==desired):std::nullopt;
    }
    bool LockerReady(){
        return IsAlive(locker.Get()) && Bool(active,locker.Get());
    }
    std::optional<std::string> ReadWardrobeSelection(size_t category){
        if(category>=2 || !selection[category])return {};
        bool ok=false;
        Root value(api,api.Call(selection[category],nullptr,nullptr,&ok));
        if(!ok)return {};
        return Text(value.Get());
    }
    void RememberWardrobeSelections(){
        if(!LockerReady())return;
        for(size_t c=0;c<2;++c){
            auto value=ReadWardrobeSelection(c);
            if(value)wardrobe_values[c]=*value;
        }
    }
    bool SetWardrobeSelection(size_t category,const std::string& desired){
        if(category>=2 || !wardrobe_select[category] || !LockerReady())return false;
        Root value(api,reinterpret_cast<Object*>(api.string_new(desired.c_str())));
        if(!value.Get())return false;
        void* args[]={value.Get()};bool ok=false;
        api.Call(wardrobe_select[category],locker.Get(),args,&ok);
        if(!ok)return false;
        auto actual=ReadWardrobeSelection(category);
        if(!actual || *actual!=desired)return false;
        wardrobe_values[category]=*actual;
        return true;
    }
    bool AlignLocalEquipped(size_t category,const std::string& desired){
        if(category>=2)return false;
        Root profile(api,Call(profile_get));
        if(!profile.Get())return false;
        Root equipped(api,Call(equipped_get,profile.Get()));
        if(!equipped.Get()||api.object_get_class(equipped.Get())!=equipped_class)return false;
        bool ok=false;
        Root current(api,api.Call(equipped_read[category],equipped.Get(),nullptr,&ok));
        const auto text=ok?Text(current.Get()):std::nullopt;
        if(text&&*text==desired)return true;
        Root replacement(api,reinterpret_cast<Object*>(api.string_new(desired.c_str())));
        if(!replacement.Get())return false;
        void* args[]={replacement.Get()};
        api.Call(equipped_write[category],equipped.Get(),args,&ok);
        if(!ok)return false;
        Root check(api,api.Call(equipped_read[category],equipped.Get(),nullptr,&ok));
        const auto actual=ok?Text(check.Get()):std::nullopt;
        return actual&&*actual==desired;
    }
    std::optional<std::string> Equipped(size_t category){
        if(category>=2)return {};
        Root profile(api,Call(profile_get));
        if(!profile.Get())return {};
        Root equipped(api,Call(equipped_get,profile.Get()));
        if(!equipped.Get()||api.object_get_class(equipped.Get())!=equipped_class)return {};
        bool ok=false;
        Root current(api,api.Call(equipped_read[category],equipped.Get(),nullptr,&ok));
        return ok?Text(current.Get()):std::nullopt;
    }
    bool WardrobeSelectionMatches(size_t category,const std::string& desired){
        if(category>=2)return false;
        if(LockerReady()){
            auto current=ReadWardrobeSelection(category);
            if(!current)return false;
            wardrobe_values[category]=*current;
            return *current==desired;
        }
        return wardrobe_values[category] && *wardrobe_values[category]==desired;
    }
    std::optional<std::string> CurrentWardrobeSelection(size_t category){
        if(category>=2)return {};
        if(LockerReady()){
            auto current=ReadWardrobeSelection(category);
            if(current)wardrobe_values[category]=*current;
            return current;
        }
        return wardrobe_values[category];
    }
    // Oyunun kendi yerel yükleyicisini (PlayerCosmeticsLoader.Load) çağırır: yalnız KENDİ oyuncu nesnen.
    // Oyun içi: PhotonView.IsMine; bekleme odası: PlayerLobbyScript.ownerActorId == yerel ActorNumber.
    int ApplyLocal(Object* local,const std::string& accessory,const std::string& skin){
        if(!loader_load||!find)return -1;
        int applied=0;
        auto each=[&](Class* klass,auto&& visit){
            const auto* type=api.class_get_type(klass);Root managed_type(api,type?api.type_get_object(type):nullptr);
            if(!managed_type.Get())return;
            int excludeInactive=0,noSorting=0;void* args[]={managed_type.Get(),&excludeInactive,&noSorting};bool ok=false;
            Root array(api,api.Call(find,nullptr,args,&ok));if(!ok||!array.Get())return;
            Root count(api,Call(array_length,array.Get()));if(!count.Get())return;
            const int n=*static_cast<int*>(api.object_unbox(count.Get()));if(n<0||n>64)return;
            for(int i=0;i<n;++i){void* index[]={&i};Object* v=api.Call(array_item,array.Get(),index,&ok);if(ok&&IsAlive(v))visit(v);}
        };
        auto load=[&](Object* loader){
            Root a(api,reinterpret_cast<Object*>(api.string_new(accessory.c_str()))),k(api,reinterpret_cast<Object*>(api.string_new(skin.c_str())));
            if(!a.Get()||!k.Get())return;
            void* args[]={a.Get(),k.Get()};bool ok=false;api.Call(loader_load,loader,args,&ok);if(ok)++applied;
        };
        each(loader_class,[&](Object* loader){
            Object* pv=Ref(loader,"pv");if(!pv||!is_mine||!IsAlive(pv)||!Bool(is_mine,pv))return;
            load(loader);
        });
        int actor=-1;
        if(local&&actor_number){Root r(api,Call(actor_number,local));if(r.Get())actor=*static_cast<int*>(api.object_unbox(r.Get()));}
        if(actor>0)each(lobby_class,[&](Object* lobby){
            auto* f=api.class_get_field_from_name(api.object_get_class(lobby),"ownerActorId");int owner_id=-1;
            if(f)api.field_get_value(lobby,f,&owner_id);
            if(owner_id!=actor)return;
            Object* loader=Ref(lobby,"playerCosmeticsLoader");if(loader&&IsAlive(loader))load(loader);
        });
        return applied;
    }
    // Çık-gir: diğer cihazlar kıyafeti yalnız oyuncu odaya girerken okur (PlayerCosmeticsLoader.Awake).
    // Yalnız Photon'un normal LeaveRoom/JoinRoom çağrıları; oda kapalı/tek kişilik ise denenmez.
    size_t ActiveCount(Class* klass){
        if(!klass||!find)return 0;
        const auto* type=api.class_get_type(klass);Root managed_type(api,type?api.type_get_object(type):nullptr);
        if(!managed_type.Get())return 0;
        int excludeInactive=0,noSorting=0;void* args[]={managed_type.Get(),&excludeInactive,&noSorting};bool ok=false;
        Root array(api,api.Call(find,nullptr,args,&ok));if(!ok||!array.Get())return 0;
        Root count(api,Call(array_length,array.Get()));if(!count.Get())return 0;
        const int n=*static_cast<int*>(api.object_unbox(count.Get()));size_t active_n=0;
        for(int i=0;i<n&&i<64;++i){void* index[]={&i};Object* v=api.Call(array_item,array.Get(),index,&ok);if(ok&&IsAlive(v)&&Bool(active,v))++active_n;}
        return active_n;
    }
    void StartRejoin(double now,Object* room){
        if(lobby_class&&ActiveCount(lobby_class)==0){wardrobe_bridge::SetStatus(u8"Çık-gir yalnız bekleme odasında çalışır; maç sırasında çıkarsan oyundan atılırsın. Oyun içinde seçim yalnız senin ekranında görünür.");return;}
        if(!leave_room||!join_room||!in_room||!room_name||!room_count||!room_open){wardrobe_bridge::SetStatus(u8"Çık-gir: gerekli oyun metotları bulunamadı.");return;}
        if(!room||!Bool(in_room)){wardrobe_bridge::SetStatus(u8"Çık-gir: şu an bir odada değilsin.");return;}
        const auto name=Text(Call(room_name,room));
        if(!name||name->empty()){wardrobe_bridge::SetStatus(u8"Çık-gir: oda adı okunamadı.");return;}
        Root count(api,Call(room_count,room));
        if(!count.Get()||*static_cast<int*>(api.object_unbox(count.Get()))<2){wardrobe_bridge::SetStatus(u8"Çık-gir: odada yalnızsın; çıkarsan oda kapanır, yeniden giremezsin.");return;}
        if(!Bool(room_open,room)){wardrobe_bridge::SetStatus(u8"Çık-gir: oda kapalı; çıkarsan geri giremezsin.");return;}
        bool leave_inactive=false,ok=false;void* args[]={&leave_inactive};
        Root r(api,api.Call(leave_room,nullptr,args,&ok));
        if(!ok){wardrobe_bridge::SetStatus(u8"Çık-gir: odadan çıkış çağrısı başarısız.");return;}
        rejoin=1;rejoin_name=*name;rejoin_deadline=now+25;rejoin_next=0;
        wardrobe_bridge::SetStatus(u8"Çık-gir: odadan çıkılıyor...");
        ROOM_LOG("v22f rejoin: LeaveRoom invoked");
    }
    void StepRejoin(double now){
        if(now>rejoin_deadline){rejoin=0;wardrobe_bridge::SetStatus(u8"Çık-gir: süre doldu; odaya geri girilemedi. Oyun menüsünden elle gir.");ROOM_LOG("v22f rejoin: timeout");return;}
        if(rejoin==1){
            if(now<rejoin_next||Bool(in_room)||!Bool(connected))return;
            rejoin_next=now+0.5;
            Root name(api,reinterpret_cast<Object*>(api.string_new(rejoin_name.c_str())));
            if(!name.Get())return;
            bool ok=false;void* args[]={name.Get(),nullptr};
            Root r(api,api.Call(join_room,nullptr,args,&ok));
            if(ok&&r.Get()&&*static_cast<bool*>(api.object_unbox(r.Get()))){rejoin=2;rejoin_deadline=now+20;wardrobe_bridge::SetStatus(u8"Çık-gir: odaya yeniden giriliyor...");ROOM_LOG("v22f rejoin: JoinRoom invoked");}
        }else if(rejoin==2&&Bool(in_room)){
            rejoin=0;wardrobe_bridge::SetStatus(u8"Çık-gir: yeniden girildi; seçimin odaya girişte diğerlerine gider.");ROOM_LOG("v22f rejoin: joined");
        }
    }
    void Tick(double now){
        if(disabled||busy)return;
        if(!started){started=true;start=now+3;return;}
        if(now<start)return;
        if(now<next_poll)return;
        next_poll=now+0.1;
        struct Guard{bool& f;explicit Guard(bool& b):f(b){f=true;}~Guard(){f=false;}}guard(busy);
        if(!initialized){initialized=true;if(!Resolve()){disabled=true;ROOM_LOG("v13 room: metadata unavailable; existing wardrobe/keyboard retained");return;}}
        Root account(api,Call(uid));const auto account_text=Text(account.Get());
        // UID is retained only as an account/session boundary. Clothing choices
        // themselves are read from the active LockerManager below.
        if(!account_text||account_text->empty()){
            plan.Context(owner,false,0,0);Suspend();return;
        }
        const auto& current_owner=*account_text;
        if(owner_known&&current_owner!=owner){
            wait_clean={true,true};
            wardrobe_values={};
        }
        owner_known=true;owner=current_owner;
        Root local(api,Call(player)),current_room(api,Call(room));
        const bool ready=Bool(connected)&&local.Get();
        plan.Context(current_owner,ready,reinterpret_cast<uintptr_t>(local.Get()),reinterpret_cast<uintptr_t>(current_room.Get()));
        if(locker.Get()&&(!IsAlive(locker.Get())||!Bool(active,locker.Get())))locker.Set(nullptr);
        if(manager.Get()&&(!IsAlive(manager.Get())||!Bool(active,manager.Get())))manager.Set(nullptr);
        if(now>=next_discovery){
            next_discovery=now+2;
            if(!LockerReady())locker.Set(FindOne(locker_class));
            if(!IsAlive(manager.Get())||!Bool(active,manager.Get()))manager.Set(FindOne(manager_class));
        }
        RememberWardrobeSelections();
        {
            const auto request=wardrobe_bridge::Take();
            if(request.pending){
                if(request.category>=2){
                    wardrobe_bridge::SetStatus(u8"Gardırop: geçersiz kıyafet kategorisi.");
                }else if(!LockerReady()){
                    // Keep the request pending until the game's real wardrobe is open.
                    wardrobe_bridge::Select(request.category,request.id);
                    wardrobe_bridge::SetStatus(u8"Önce oyunun gardırobunu aç; seçim oradaki normal kıyafet kontrolünden yapılacak.");
                }else if(!SetWardrobeSelection(request.category,request.id)){
                    wardrobe_bridge::SetStatus(u8"Gardırop seçimi kabul etmedi; oyun içindeki kilit/sahiplik koşulları korunuyor.");
                }else{
                    plan.Select(request.category,request.id);
                    reporting[request.category]=true;refresh[request.category]=true;
                    wardrobe_bridge::SetSelected(request.category,request.id);
                    wardrobe_bridge::SetStatus(u8"Gardırop seçimi uygulandı; oda yayını sıraya alındı.");
                    ROOM_LOG("v23 wardrobe: native LockerManager accepted %s selection",request.category==0?"accessory":"skin");
                }
            }
        }
        if(wardrobe_bridge::TakeRejoin()&&rejoin==0)StartRejoin(now,current_room.Get());
        if(rejoin!=0)StepRejoin(now);
        if(LockerReady()){
            // Snapshot the live wardrobe's two selections. Never substitute the
            // profile's owned/equipped lists when the wardrobe is unavailable.
            RememberWardrobeSelections();
            // The game's flm/fln handlers set these flags on actual item clicks.
            // fll resets both flags when opening the wardrobe; ignore that reset.
            for(size_t c=0;c<2;++c){
                bool changed=false;api.field_get_value(locker.Get(),dirty[c],&changed);
                if(!changed)wait_clean[c]=false;
                if(wait_clean[c])continue;
                if(!changed)continue;
                bool read_ok=false;Root selected(api,api.Call(selection[c],nullptr,nullptr,&read_ok));
                if(!read_ok)continue;
                auto value=Text(selected.Get());
                if(value&&(!plan.Wanted(c)||*plan.Wanted(c)!=*value)){
                    plan.Select(c,*value);ROOM_LOG("v13 room: selected %s retained for own-player publication",c==0?"accessory":"skin");
                }
            }
        }
        // Only a selection observed through LockerManager is authoritative here.
        // Do not reconstruct or overwrite it from account-profile equipped fields.
        std::array<bool,2> aligned{};
        for(size_t c=0;c<2;++c)if(plan.Wanted(c)){
            // Never source the desired item from the profile: it came from the
            // actual LockerManager selection. Mirror it into the existing local
            // equipped state so the game's own player loader sees the same choice.
            aligned[c]=WardrobeSelectionMatches(c,*plan.Wanted(c)) &&
                       AlignLocalEquipped(c,*plan.Wanted(c));
        }
        if(refresh[0]||refresh[1]){
            bool ready_to_draw=true;
            for(size_t c=0;c<2;++c)if(refresh[c]&&plan.Wanted(c)&&!aligned[c])ready_to_draw=false;
            if(ready_to_draw){
                auto acc=plan.Wanted(0)?plan.Wanted(0):CurrentWardrobeSelection(0);
                auto skin=plan.Wanted(1)?plan.Wanted(1):CurrentWardrobeSelection(1);
                if(acc&&skin){
                    const int n=ApplyLocal(local.Get(),*acc,*skin);last_applied=n;
                    ROOM_LOG("v23 wardrobe: native wardrobe selection refreshed on %d own-player object(s)",n);
                    wardrobe_bridge::SetStatus(n>0?u8"Gardırop seçimi yerel oyuncuda yenilendi.":u8"Yerel görünüm yenilenemedi: kendi oyuncu nesnen bulunamadı.");
                }
                refresh={};
            }
        }
        auto report=[&](size_t c,const char* text){if(reporting[c]){wardrobe_bridge::SetStatus(text);}};
        if(!ready||!IsAlive(manager.Get())||!Bool(active,manager.Get())){
            for(size_t c=0;c<2;++c){
                if(!ready)report(c,u8"Takıldı: Photon hazır değil (bağlı değil ya da yerel oyuncu yok).");
                else if(last_applied>0)report(c,u8"Yerel görünüm yenilendi. Bu sahnede yayın nesnesi yok: diğer oyuncular değişikliği görmez.");
                else report(c,u8"Takıldı: oyunun PhotonManager nesnesi bulunamadı (odada tek etkin kopya aranıyor).");
            }
            return;
        }
        for(size_t c=0;c<2;++c){
            if(!plan.Wanted(c))continue;
            if(!aligned[c]){report(c,u8"Gardırop seçimi okunamadı veya mevcut seçimle eşleşmiyor.");continue;}
            const auto match=Matches(local.Get(),c,*plan.Wanted(c));
            if(!plan.Due(c,now,match)){report(c,match&&*match?u8"Yerel özellik zaten bu seçimde; yayın gerekmedi.":u8"Yayın zamanı bekleniyor...");continue;}
            reporting[c]=false;
            Root value(api,reinterpret_cast<Object*>(api.string_new(plan.Wanted(c)->c_str())));
            bool ok=false;
            if(value.Get()){void* args[]={value.Get()};api.Call(publish[c],manager.Get(),args,&ok);}
            plan.Attempt(c,now,ok);
            const auto after=Matches(local.Get(),c,*plan.Wanted(c));
            wardrobe_bridge::SetStatus(std::string(c==0?u8"Şapka":u8"Karakter")+(ok?u8": oyunun yayıncısı çağrıldı, ":u8": yayın çağrısı başarısız, ")+
                (after?(*after?u8"yerel özellik eşleşti.":u8"yerel özellik eşleşmedi."):u8"yerel özellik okunamadı."));
            ROOM_LOG("v13 room: %s original publisher=%s local-property=%s; remote receipt unverified",c==0?"accessory":"skin",ok?"invoked":"failed",after?( *after?"matched":"not matched"):"unknown");
        }
    }
    void Suspend(){locker.Set(nullptr);manager.Set(nullptr);}
};
}
void Pump(const il2cpp::Api& api,bool paused){
    static Runtime* runtime=nullptr;static bool stopped=false;if(stopped)return;
    try{
        if(paused){if(runtime)runtime->Suspend();return;}
        if(!runtime)runtime=new Runtime(api);
        const auto now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();runtime->Tick(now);
    }catch(...){stopped=true;ROOM_LOG("v13 room: native exception; publication stopped; existing UI retained");}
}
}
