#include "cosmetic_catalog.h"
#include "wardrobe_bridge.h"
#include <android/log.h>
#include <algorithm>
#include <chrono>
#include <utility>

#define COS_LOG(...) __android_log_print(ANDROID_LOG_INFO, "multiplayermod", __VA_ARGS__)
namespace cosmetic_catalog {
namespace {
constexpr const char* fields[] = {"skins", "accessories", "emotes"};
constexpr const char* getters[] = {"crr", "crt", "crv"};
constexpr const char* setters[] = {"crs", "cru", "crw"};
constexpr const char* backing[] = {"<ojw>k__BackingField", "<ojx>k__BackingField", "<ojy>k__BackingField"};
constexpr int kMaxEntries = 512;
struct Root {
    const il2cpp::Api& api; il2cpp::Handle handle;
    Root(const il2cpp::Api& a, il2cpp::Object* o): api(a), handle(o ? a.gchandle_new(o,false) : 0) {}
    ~Root() { if (handle) api.gchandle_free(handle); }
    il2cpp::Object* Get() const { return handle ? api.gchandle_get_target(handle) : nullptr; }
};
}
Controller::~Controller() { ClearSources(); ClearTargets(); }
void Controller::Free(Handle& h) { if (h) api_.gchandle_free(h); h=0; }
void Controller::ClearSources() { for (auto& s:sources_) { Free(s.list); s={}; } }
void Controller::ClearTargets() {
    Free(profile_);
    for (auto& t:targets_) { Free(t.list); t={}; }
    for (auto& v:inserted_) v.clear();
    wardrobe_bridge::ClearLists();
    wardrobe_bridge::SetStatus("Gardırop listesi bekleniyor; oyun gardırobunu açın.");
}
Controller::Object* Controller::Call0(const Method* method, Object* self) {
    bool ok=false; auto* result=api_.Call(method,self,nullptr,&ok); return ok ? result : nullptr;
}
bool Controller::Integer(const Method* method, Object* self, int& value) {
    bool ok=false; auto* result=api_.Call(method,self,nullptr,&ok);
    if (!ok || !result) return false;
    value=*static_cast<int*>(api_.object_unbox(result)); return true;
}
bool Controller::Boolean(const Method* method, Object* self, Object* value, bool& result) {
    void* args[]={value}; bool ok=false;
    auto* boxed=api_.Call(method,self,args,&ok);
    if (!ok || !boxed) return false;
    result=*static_cast<bool*>(api_.object_unbox(boxed)); return true;
}
Controller::Object* Controller::Read(Object* self, const char* name) {
    if (!self) return nullptr;
    auto* field=api_.class_get_field_from_name(api_.object_get_class(self),name);
    Object* value=nullptr;
    if (field) api_.field_get_value(self,field,&value); // Getter always takes output address.
    return value;
}
bool Controller::Resolve() {
    if (!api_.field_get_value || !api_.class_get_field_from_name || !api_.object_get_class ||
        !api_.gchandle_new || !api_.gchandle_get_target || !api_.gchandle_free) return false;
    library_class_=api_.Find("","CosmeticLibrary"); profile_class_=api_.Find("","ey");
    string_class_=api_.Find("System","String");
    library_get_=api_.FindMethod(api_.Find("","kt"),"eql",0);
    cache_class_=api_.Find("","fi");
    profile_get_=api_.FindMethod(cache_class_,"cvf",0);
    // CosmeticsLoader.OnEnable subscribes to fi.cux; OnDisable removes the
    // handler from this Action field. Resolve metadata, never its native offset.
    changed_event_=cache_class_ ? api_.class_get_field_from_name(cache_class_,"oly") : nullptr;
    action_class_=api_.Find("System","Action");
    changed_invoke_=api_.FindMethod(action_class_,"Invoke",0);
    constexpr const char* view_getters[]={"cvz","cwa","cwb"};
    for (size_t c=0;c<3;++c) view_get_[c]=api_.FindMethod(cache_class_,view_getters[c],0);
    for (size_t c=0;c<3;++c) lists_get_[c]=api_.FindMethod(profile_class_,getters[c],0);
    return library_class_ && profile_class_ && string_class_ && library_get_ && profile_get_ &&
        view_get_[0] && view_get_[1] &&
        std::all_of(lists_get_.begin(),lists_get_.end(),[](const Method* m){return m!=nullptr;});
}
Controller::Object* Controller::Library() {
    if (Object* library=Call0(library_get_))
        return api_.object_get_class(library)==library_class_ ? library : nullptr;
    // The shop's static reference may not be assigned until its screen opens.
    // Discover only already-loaded assets; never load a bundle or instantiate a prefab.
    if (!api_.class_get_type || !api_.type_get_object) return nullptr;
    const auto* type=api_.class_get_type(library_class_);
    Root managed_type(api_,type ? api_.type_get_object(type) : nullptr);
    if (!managed_type.Get()) return nullptr;
    const auto* find=api_.FindMethod(api_.Find("UnityEngine","Resources"),"FindObjectsOfTypeAll",1);
    void* args[]={managed_type.Get()}; bool ok=false;
    Root array(api_,api_.Call(find,nullptr,args,&ok));
    if (!ok || !array.Get()) return nullptr;
    auto* klass=api_.object_get_class(array.Get());
    int length=0;
    if (!Integer(api_.FindMethod(klass,"get_Length",0),array.Get(),length) || length!=1) return nullptr;
    int zero=0; void* index[]={&zero};
    // Array.GetValue(int) also has an Int64 overload. GetValueImpl has an exact Int32 ABI.
    Object* result=api_.Call(api_.FindMethod(klass,"GetValueImpl",1),array.Get(),index,&ok);
    return ok && result && api_.object_get_class(result)==library_class_ ? result : nullptr;
}
bool Controller::StartScan(Object* library) {
    ClearSources();
    for (auto& v:ids_) v.clear();
    Root library_root(api_,library);
    if (!library_root.Get()) return false;
    for (size_t c=0;c<3;++c) {
        auto* list=Read(library_root.Get(),fields[c]);
        auto& s=sources_[c]; s.list=list ? api_.gchandle_new(list,false) : 0;
        if (!s.list) { ClearSources(); return false; }
        auto* klass=api_.object_get_class(api_.gchandle_get_target(s.list));
        s.count=api_.FindMethod(klass,"get_Count",0); s.item=api_.FindMethod(klass,"get_Item",1);
        if (!s.item || !Integer(s.count,api_.gchandle_get_target(s.list),s.length) ||
            s.length<0 || s.length>kMaxEntries) { ClearSources(); return false; }
    }
    category_=index_=0; status_.phase=Phase::Scan; return true;
}
bool Controller::Scan() {
    int budget=32;
    while (category_<3 && budget-->0) {
        auto& s=sources_[category_];
        if (s.at==s.length) { ++category_; continue; }
        int count=0; Object* list=api_.gchandle_get_target(s.list);
        if (!Integer(s.count,list,count) || count!=s.length) return false;
        int index=s.at++; void* args[]={&index}; bool ok=false;
        Root entry(api_,api_.Call(s.item,list,args,&ok));
        if (!ok) return false;
        if (!entry.Get()) continue; // Unassigned inspector slots are not catalogue IDs.
        Root id(api_,Read(entry.Get(),"Id"));
        if (!id.Get()) continue;
        auto* string=reinterpret_cast<il2cpp::String*>(id.Get());
        if (api_.object_get_class(id.Get())!=string_class_) return false;
        const int units=api_.string_length(string);
        if (units<0 || units>256) return false;
        const auto* chars=api_.string_chars(string);
        if (!chars && units) return false;
        for (int i=0;i<units;++i) if (!chars[i]) return false;
        const auto value=api_.Utf8(string);
        auto& ids=ids_[category_];
        if (!value.empty() && std::find(ids.begin(),ids.end(),value)==ids.end()) ids.push_back(value);
    }
    if (category_==3) {
        ClearSources();
        size_t total=0;
        for (size_t c=0;c<3;++c) { status_.catalog[c]=ids_[c].size(); total+=ids_[c].size(); }
        if (!total) return false;
        catalog_ready_=true; category_=index_=0; status_.phase=Phase::Waiting;
        COS_LOG("v12 cosmetics: catalog ready skins=%zu accessories=%zu emotes=%zu",status_.catalog[0],status_.catalog[1],status_.catalog[2]);
    }
    return true;
}
Controller::Object* Controller::List(size_t category,Object* profile) {
    bool ok=false;
    auto* existing=api_.Call(lists_get_[category],profile,nullptr,&ok);
    if (!ok) return nullptr;
    if (existing) return existing;
    // An account with no purchases may have a null list. Resolve its declared
    // List<string> type and use the existing constructor/property setter ABI.
    if (!api_.field_get_type || !api_.class_from_type || !api_.object_new) return nullptr;
    auto* field=api_.class_get_field_from_name(profile_class_,backing[category]);
    const auto* type=field ? api_.field_get_type(field) : nullptr;
    auto* klass=type ? api_.class_from_type(type) : nullptr;
    const auto* ctor=api_.FindMethod(klass,".ctor",0);
    const auto* setter=api_.FindMethod(profile_class_,setters[category],1);
    if (!ctor || !setter) return nullptr;
    Root created(api_,api_.object_new(klass));
    if (!created.Get()) return nullptr;
    api_.Call(ctor,created.Get(),nullptr,&ok);
    if (!ok) return nullptr;
    void* args[]={created.Get()}; api_.Call(setter,profile,args,&ok);
    if (ok && Call0(lists_get_[category],profile)==created.Get()) return created.Get();
    void* clear[]={nullptr}; api_.Call(setter,profile,clear); // Restore the original null value.
    return nullptr;
}
bool Controller::Begin(Object* profile) {
    ClearTargets(); status_.added={};
    if (!profile || api_.object_get_class(profile)!=profile_class_) return false;
    profile_=api_.gchandle_new(profile,false);
    if (!profile_) return false;
    for (size_t c=0;c<3;++c) {
        Object* list=List(c,api_.gchandle_get_target(profile_));
        auto& t=targets_[c]; t.list=list ? api_.gchandle_new(list,false) : 0;
        if (!t.list) { ClearTargets(); return false; }
        auto* klass=api_.object_get_class(api_.gchandle_get_target(t.list));
        t.count=api_.FindMethod(klass,"get_Count",0); t.contains=api_.FindMethod(klass,"Contains",1);
        t.add=api_.FindMethod(klass,"Add",1); t.remove=api_.FindMethod(klass,"Remove",1);
        if (!t.contains || !t.add || !t.remove || !Integer(t.count,list,t.final_count) ||
            t.final_count<0 || t.final_count>4096) { ClearTargets(); return false; }
        for (size_t previous=0;previous<c;++previous)
            if (api_.gchandle_get_target(targets_[previous].list)==list) { ClearTargets(); return false; }
    }
    category_=index_=0; status_.phase=Phase::Apply; return true;
}
bool Controller::SameProfile() {
    Object* current=Call0(profile_get_);
    if (!profile_ || current!=api_.gchandle_get_target(profile_)) return false;
    for (size_t c=0;c<3;++c)
        if (Call0(lists_get_[c],current)!=api_.gchandle_get_target(targets_[c].list)) return false;
    return true;
}
bool Controller::Apply() {
    if (!SameProfile()) return false;
    int budget=8;
    while (category_<3 && budget-->0) {
        auto& ids=ids_[category_];
        if (index_==ids.size()) { ++category_; index_=0; continue; }
        const auto& id=ids[index_++];
        Root string(api_,reinterpret_cast<Object*>(api_.string_new(id.c_str())));
        Object* list=api_.gchandle_get_target(targets_[category_].list);
        bool contains=false;
        if (!string.Get() || !Boolean(targets_[category_].contains,list,string.Get(),contains)) return false;
        if (!contains) {
            // Record before invocation: a failing invocation may already have mutated the list.
            inserted_[category_].push_back(id);
            void* args[]={string.Get()}; bool ok=false;
            api_.Call(targets_[category_].add,list,args,&ok);
            if (!ok || !Boolean(targets_[category_].contains,list,string.Get(),contains) || !contains) return false;
            ++status_.added[category_];
        }
    }
    if (category_==3) {
        for (size_t c=0;c<3;++c)
            if (!Integer(targets_[c].count,api_.gchandle_get_target(targets_[c].list),targets_[c].final_count)) return false;
        for (auto& v:inserted_) v.clear();
        status_.phase=Phase::Ready;
        COS_LOG("v12 cosmetics: source lists committed; added=%zu/%zu/%zu; remote visibility unverified",status_.added[0],status_.added[1],status_.added[2]);
        // CosmeticsLoader.OnEnable/its change listener rebuilds the actual locker
        // cards from fi.cwb (emotes), fi.cwa (accessories), and fi.cvz (skins).
        NotifyChanged();
        RefreshWardrobeLists();
    }
    return true;
}
void Controller::NotifyChanged() {
    if (!changed_event_ || !changed_invoke_ || !action_class_ || !api_.field_static_get_value) {
        COS_LOG("v12 cosmetics: change notification unavailable; reopen wardrobe"); return;
    }
    Object* callback=nullptr;
    api_.field_static_get_value(changed_event_,&callback);
    if (!callback) { COS_LOG("v12 cosmetics: no active change listeners; open wardrobe"); return; }
    Root listener(api_,callback);
    if (!listener.Get() || api_.object_get_class(listener.Get())!=action_class_) {
        COS_LOG("v12 cosmetics: unexpected change-listener type; notification skipped"); return;
    }
    // A listener can rebuild UI, reload a profile, or re-enter an update. Publish
    // Ready first, hold the delegate alive, and suppress recursive catalogue work.
    struct Guard { bool& flag; explicit Guard(bool& f):flag(f){flag=true;} ~Guard(){flag=false;} } guard(notifying_);
    bool ok=false; api_.Call(changed_invoke_,listener.Get(),nullptr,&ok);
    COS_LOG("v12 cosmetics: change notification %s",ok ? "delivered" : "failed; reopen wardrobe");
}
bool Controller::ReadWardrobeList(size_t category,std::vector<std::string>& out) {
    out.clear();
    if (category>=view_get_.size() || !view_get_[category]) return false;
    // These fi getters are the exact list sources passed by CosmeticsLoader.haz
    // into CosmeticsLoader.hba before the game instantiates each wardrobe card.
    Root list(api_,Call0(view_get_[category]));
    if (!list.Get()) return false;
    auto* klass=api_.object_get_class(list.Get());
    const auto* count_method=api_.FindMethod(klass,"get_Count",0);
    const auto* item_method=api_.FindMethod(klass,"get_Item",1);
    int count=0;
    if (!count_method || !item_method ||
        !Integer(count_method,list.Get(),count) || count<0 || count>kMaxEntries) return false;

    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(count));
    for (int i=0;i<count;++i) {
        void* args[]={&i}; bool ok=false;
        Root item(api_,api_.Call(item_method,list.Get(),args,&ok));
        if (!ok || !item.Get() || api_.object_get_class(item.Get())!=string_class_) return false;
        auto* value=reinterpret_cast<il2cpp::String*>(item.Get());
        const int units=api_.string_length(value);
        if (units<0 || units>256) return false;
        const auto* chars=api_.string_chars(value);
        if (!chars && units) return false;
        for (int j=0;j<units;++j) if (!chars[j]) return false;
        std::string id=api_.Utf8(value);
        if (id.empty()) continue;
        if (std::find(result.begin(),result.end(),id)==result.end()) result.push_back(std::move(id));
    }
    out=std::move(result);
    return true;
}
bool Controller::RefreshWardrobeLists() {
    std::vector<std::string> skins,accessories;
    // ImGui has skin and accessory tabs. Emotes are read by the game's loader too,
    // but are not exposed by this two-tab panel.
    if (!ReadWardrobeList(0,skins) || !ReadWardrobeList(1,accessories) ||
        (skins.empty()&&!ids_[0].empty()) || (accessories.empty()&&!ids_[1].empty())) {
        // Keep last valid snapshot for this profile while the view rebuilds.
        wardrobe_bridge::SetStatus("Gardırop listesi henüz hazır değil. Oyun içi gardırop ekranını açın.");
        COS_LOG("v12 cosmetics: wardrobe card source unavailable; no account-list fallback used");
        return false;
    }
    const size_t skin_count=skins.size(),accessory_count=accessories.size();
    wardrobe_bridge::SetWardrobeLists(std::move(accessories),std::move(skins));
    wardrobe_bridge::SetStatus("Gardırop listesi okundu: şapka="+std::to_string(accessory_count)+
        ", karakter="+std::to_string(skin_count));
    COS_LOG("v12 cosmetics: live wardrobe lists skins=%zu accessories=%zu; catalog=%zu/%zu/%zu",
        skin_count,accessory_count,status_.catalog[0],status_.catalog[1],status_.catalog[2]);
    return true;
}
void Controller::Rollback() {
    int budget=8;
    for (size_t c=0;c<3;++c) {
        auto& items=inserted_[c];
        while (!items.empty() && budget-->0) {
            const auto id=items.back(); items.pop_back();
            Root value(api_,reinterpret_cast<Object*>(api_.string_new(id.c_str())));
            bool removed=false;
            if (!value.Get() || !Boolean(targets_[c].remove,api_.gchandle_get_target(targets_[c].list),value.Get(),removed))
                COS_LOG("v12 cosmetics: rollback incomplete; restart game to reload original lists");
        }
        if (!items.empty()) return;
    }
    ClearTargets(); status_.added={};
    status_.phase=retry_after_rollback_?Phase::Waiting:Phase::Disabled;
    retry_after_rollback_=false;
    COS_LOG("v12 cosmetics: rollback complete; phase=%s",status_.phase==Phase::Waiting?"retry":"disabled");
}
void Controller::Wait(double now,const char* reason) {
    ClearSources(); status_.phase=Phase::Waiting; next_=now+2.0;
    if (++attempts_<=3) COS_LOG("v12 cosmetics: waiting for %s; open wardrobe/shop",reason);
}
void Controller::Fail(const char* reason) {
    COS_LOG("v12 cosmetics: %s; no purchase/currency request made",reason);
    status_.phase=Phase::Rollback;
}
void Controller::Tick(double now) {
    if (notifying_) return;
    if (status_.phase==Phase::Disabled) return;
    if (!started_) { started_=true; next_=now+3.0; return; }
    if (status_.phase==Phase::Rollback) { Rollback(); return; }
    if (status_.phase==Phase::Scan) {
        if (!Scan()) { catalog_ready_=false; Wait(now,"complete catalog"); }
        return;
    }
    if (status_.phase==Phase::Apply) {
        if(!SameProfile()){retry_after_rollback_=true;next_=now+2.0;Fail("active profile changed; retry after rollback");}
        else if (!Apply()) Fail("apply failed");
        if (status_.phase==Phase::Ready) {next_=now+2.0;next_notify_=now+4.0;}
        return;
    }
    if (now<next_) return;
    next_=now+2.0;
    if (!initialized_) {
        initialized_=true;
        if (!Resolve()) { status_.phase=Phase::Disabled; COS_LOG("v12 cosmetics: metadata/API unavailable; keyboard remains active"); return; }
    }
    if (!catalog_ready_) {
        Object* library=Library();
        if (!library || !StartScan(library)) Wait(now,"catalog asset");
        return;
    }
    if (status_.phase==Phase::Ready && SameProfile()) {
        // Poll the real card-source getters at a low rate. The locker can rebuild
        // after its screen opens/closes without requiring a profile/account reread.
        bool unchanged=true;
        for (auto& t:targets_) {
            int count=0;
            if (!Integer(t.count,api_.gchandle_get_target(t.list),count) || count!=t.final_count) unchanged=false;
        }
        if (unchanged) {
            if(!RefreshWardrobeLists()&&now>=next_notify_){
                next_notify_=now+4.0;
                NotifyChanged();
                RefreshWardrobeLists();
            }
            return;
        }
    }
    Object* profile=Call0(profile_get_);
    if (!profile || !Begin(profile)) { ClearTargets(); Wait(now,"signed-in cosmetic lists"); }
}
void Pump(const il2cpp::Api& api) {
    // Process lifetime, as for the installed hooks. Never free GC roots from a C++
    // static destructor after Unity has already torn its VM down.
    static Controller* controller=nullptr;
    static bool stopped=false;
    if (stopped) return;
    try {
        if (!controller) controller=new Controller(api);
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        controller->Tick(seconds);
    } catch (...) {
        stopped=true;
        COS_LOG("v12 cosmetics: native exception; cosmetic work paused; keyboard remains active");
    }
}
}
