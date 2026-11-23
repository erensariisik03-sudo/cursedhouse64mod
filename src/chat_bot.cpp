#include "chat_bot.h"
#include "voice_ai_bridge.h"
#include "chat_bot_bridge.h"
#include "chat_bot_core.h"
#include "chat_bot_fx.h"
#include "chat_send_gate.h"
#include "chat_bot_engine.h"
#include "chat_bot_http.h"
#include "keyboard_bridge.h"
#include "text_buffer.h"
#include <android/log.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#define BOT_LOG(...) __android_log_print(ANDROID_LOG_INFO, "multiplayermod", __VA_ARGS__)
namespace chat_bot {
namespace {
using namespace il2cpp;
using chat_bot_bridge::Edit;
constexpr size_t kChunkScalars = 200;  // tek sohbet satırı: bundan uzunsa cümle sonunda bölünür (satırlar üst üste binmesin)
constexpr double kChunkGap = 1.2;
constexpr double kWarningSenderGap=60.0,kWarningGlobalGap=10.0;
constexpr double kSmallTalkGap = 30.0; // yerel selam yanıtıyla sohbeti doldurma
constexpr double kPerSenderGap = 6.0;  // aynı kişiden en sık 6 sn'de bir mesaj işlenir
constexpr double kConversationWindow = 45.0; // adımızla konuşan kişinin devam mesajları bu süre cevaplanır
constexpr double kSideTalkWindow = 40.0;     // başkasına yazan/yazılan kişinin mesajları bu süre cevaplanmaz
constexpr double kMinGap = 3.0;        // iki cevap arası en az bekleme (sn)
constexpr double kOutboxTimeout = 20.0; // cooldown/odak yüzünden gönderilemezse bırak
struct Root {
    const Api& api; Handle handle = 0;
    Root(const Api& a, Object* v = nullptr) : api(a) { Reset(v); }
    ~Root() { if (handle) api.gchandle_free(handle); }
    void Reset(Object* v) { const Handle n = v ? api.gchandle_new(v, false) : 0; if (handle) api.gchandle_free(handle); handle = n; }
    Object* Get() const { return handle ? api.gchandle_get_target(handle) : nullptr; }
    Root(const Root&) = delete;
};

class Controller {
    const Api& api;
    bool initialized = false, disabled = false;
    double gap_until = 0, start = 0, next = 0, next_players = 0, next_request = 0, outbox_since = 0;
    Class *chat_message_class{}, *chat_manager_class{}, *player_class{};
    const Method *find{}, *length{}, *item{}, *alive{}, *instance_id{}, *active{}, *get_transform{}, *sibling{},
        *text_get{}, *input_text{}, *input_set{}, *input_focused{}, *send{}, *queue{}, *room{}, *players{},
        *nick{}, *own_nick{};
    Field *message_field{}, *input_field{}, *allowed_field{};
    chat_bot_engine::Engine engine;
    chat_bot_bridge::Snapshot snap;
    std::unordered_set<int> seen;
    std::vector<chat_bot_core::Turn> history;
    std::string key, custom, last_seen;
    std::vector<std::string> selected; bool all = false, strict = true, message_filter = true; int mode = 0, words = chat_bot_core::kMinWords;
    struct Pending { std::string name, text; bool canned = false; };
    std::unordered_map<std::string, double> last_accept, engaged, talking; // talking: başkalarıyla konuşan kişiler // engaged: bize yazan kişinin 'devam eden konuşma' süresi
    double now_ = 0;
    std::deque<Pending> pending;
    std::deque<std::string> outbox, recent_sent; // gönderilecek parçalar; kendi yolladıklarımız (yankı koruması)
    bool enabled = false, baseline = false, voice_routed=false;
    Edit editing = Edit::None;
    std::string issue, own_name, request_sender, reminder_name;
    chat_bot_core::NameReminder reminder;
    uintptr_t active_room=0,reminder_room=0;
    bool outbox_reminder=false;
    uint64_t reply_generation=0,request_generation=0;
    std::unordered_map<std::string,double> last_smalltalk,last_warning;
    size_t warning_chunks=0;double next_warning=0;
    void DropOutbox(){outbox.clear();warning_chunks=0;outbox_reminder=false;reminder_name.clear();reminder_room=0;}
    void InvalidateReplies(){++reply_generation;pending.clear();DropOutbox();}


    void WarnProfanity(const std::string& sender) {
        pending.erase(std::remove_if(pending.begin(),pending.end(),[&](const Pending& p){return p.name==sender;}),pending.end());
        if(engine.Busy()&&request_sender==sender&&request_generation==reply_generation){
            ++reply_generation;
            if(!history.empty()&&!history.back().model)history.pop_back();
        }
        const auto previous=last_warning.find(sender);
        if(warning_chunks||now_<next_warning||(previous!=last_warning.end()&&now_-previous->second<kWarningSenderGap))return;
        if(outbox_reminder)DropOutbox();
        const auto warnings=chat_bot_core::ProfanityWarnings(sender);
        for(auto it=warnings.rbegin();it!=warnings.rend();++it)outbox.push_front(*it);
        warning_chunks=warnings.size();outbox_since=now_;
        last_warning[sender]=now_;next_warning=now_+kWarningGlobalGap;
        Status(u8"AI | Yerel küfür uyarısı hazır; AI isteği yapılmadı.");
    }

    std::deque<std::string> status_lines;
    double status_next=0,status_deadline=0;
    uintptr_t status_room=0;
    uint64_t status_revision=0;
    std::string status_model;
    void ClearStatus(){++status_revision;status_lines.clear();status_room=0;}
    void QueueStatus(chat_bot_fx::Event event,const std::string& detail={}) {
        ++status_revision;
        if(!chat_send_gate::status_alias_ready.load(std::memory_order_acquire)){
            ClearStatus();Status(u8"AI | System sohbet etiketi hazır değil; terminal gösterimi atlandı.");return;
        }
        const auto lines=chat_bot_fx::Lines(event,detail,enabled);
        status_lines.assign(lines.begin(),lines.end()); // latest event replaces unfinished animation
        status_next=now_;status_deadline=now_+chat_bot_fx::kTimeout;status_room=active_room;
    }
    bool SendStatusLine(const std::string& line) {
        if(!chat_send_gate::status_alias_ready.load(std::memory_order_acquire))return false;
        Root owner(api,ChatOwner());if(!owner.Get())return false;
        Object* field=nullptr;api.field_get_value(owner.Get(),input_field,&field);
        Root input(api,field);if(!input.Get()||!IsAlive(input.Get()))return false;
        bool ok=false;auto* focused=api.Call(input_focused,input.Get(),nullptr,&ok);
        if(!ok||!focused||*static_cast<bool*>(api.object_unbox(focused)))return false;
        if(!chat_bot_core::Trim(ReadString(input_text,input.Get(),ok)).empty()||!ok)return false;
        bool allowed=false;api.field_get_value(owner.Get(),allowed_field,&allowed);
        if(!allowed&&!api.field_set_value)return false; // old runtimes retain ordinary cooldown
        Root alias(api,reinterpret_cast<Object*>(api.string_new("System")));
        if(!alias.Get())return false;
        Root text(api,reinterpret_cast<Object*>(api.string_new(line.c_str())));
        if(!text.Get())return false;
        void* args[]={text.Get()};api.Call(input_set,input.Get(),args,&ok);
        if(!ok)return false;
        const auto applied=ReadString(input_text,input.Get(),ok);
        if(!ok||applied!=line)return false;
        // This guard exists only around this one status SendMessage call. No
        // timer is disabled and normal replies never receive an override.
        bool sent=false;
        {
            struct Permission {
                const Api& api;Object* owner;Field* field;bool old,changed=false;
                Permission(const Api& a,Object* o,Field* f,bool before):api(a),owner(o),field(f),old(before){
                    if(!old&&api.field_set_value){changed=true;bool yes=true;api.field_set_value(owner,field,&yes);}
                }
                ~Permission(){if(changed)api.field_set_value(owner,field,&old);}
            } permission(api,owner.Get(),allowed_field,allowed);
            bool permitted=false;api.field_get_value(owner.Get(),allowed_field,&permitted);
            if(permitted){
                chat_send_gate::StatusAlias display(alias.handle);
                api.Call(send,owner.Get(),nullptr,&sent);
            }
        }
        bool read_ok=false;const auto after=ReadString(input_text,input.Get(),read_ok);
        if(read_ok&&after==line){
            Root empty(api,reinterpret_cast<Object*>(api.string_new("")));void* reset[]={empty.Get()};
            if(empty.Get()) {
                api.Call(input_set,input.Get(),reset);
            }
            return false;
        }
        if(!sent||!read_ok)return false;
        recent_sent.push_back(chat_bot_core::StripTags(line));while(recent_sent.size()>12)recent_sent.pop_front();
        return true;
    }
    void PumpStatus(double now) {
        if(status_lines.empty())return;
        if(now>status_deadline){ClearStatus();return;}
        if(now<status_next)return;
        if(!api.Value<bool>(queue,nullptr,false))return;
        Root current(api,api.Call(room));
        const auto id=reinterpret_cast<uintptr_t>(current.Get());
        if(!id||(status_room&&status_room!=id)){ClearStatus();return;}
        status_room=id;status_next=now+chat_bot_fx::kInterval;
        const auto revision=status_revision;const std::string line=status_lines.front();
        if(SendStatusLine(line)&&revision==status_revision){
            status_lines.pop_front();gap_until=now+kMinGap;
        }
    }

    std::string ConfigPath() {
        std::ifstream cmd("/proc/self/cmdline", std::ios::binary);
        std::string pkg; std::getline(cmd, pkg, '\0');
        if (pkg.empty() || pkg.find('/') != std::string::npos) return {};
        for (const std::string base : {"/data/data/", "/data/user/0/"}) {
            const std::string dir = base + pkg + "/files";
            struct stat st{};
            if (stat(dir.c_str(), &st) == 0 || mkdir(dir.c_str(), 0700) == 0) return dir + "/gmm_ai.cfg";
        }
        return {};
    }
    void LoadConfig() {
        const std::string path = ConfigPath(); if (path.empty()) return;
        std::ifstream in(path, std::ios::binary);
        if (!in) { all = true; return; } // ilk kurulum: herkese cevap (kişi seçme gerekmez)
        std::stringstream ss; ss << in.rdbuf();
        const auto c = chat_bot_core::ParseConfig(ss.str());
        key = c.key; custom = c.custom; mode = c.mode; all = c.all; strict = c.strict; words = c.words;message_filter=c.message_filter;
        if (!key.empty()) Status(u8"AI | Kayıtlı anahtar yüklendi.");
    }
    void SaveConfig() {
        const std::string path = ConfigPath(); if (path.empty()) { BOT_LOG("chat_bot: config path unavailable"); return; }
        chat_bot_core::Config c; c.key = key; c.custom = custom; c.mode = mode; c.all = all; c.strict = strict; c.words = words;c.message_filter=message_filter;
        const std::string data = chat_bot_core::SerializeConfig(c);
        std::ofstream out(path, std::ios::binary | std::ios::trunc); // uygulamanın özel dizini; anahtar loglanmaz
        if (out) { out << data; out.close(); chmod(path.c_str(), 0600); }
    }
    bool Selected(const std::string& n) const { return std::find(selected.begin(), selected.end(), n) != selected.end(); }
    std::string System() const { return chat_bot_core::BuildSystem(mode, custom, static_cast<size_t>(words),message_filter); }
    size_t MaxChunks() const { return static_cast<size_t>(std::clamp((words + 27) / 28, 2, 6)); } // ~28 kelime/satır (200 karakter sınırı)
    bool IsAlive(Object* o) {
        if (!o) return false;
        void* args[] = {o}; bool ok = false;
        auto* r = api.Call(alive, nullptr, args, &ok);
        return ok && r && *static_cast<bool*>(api.object_unbox(r));
    }
    Object* Array(Class* klass, int include_inactive = 0) {
        if (!klass || !api.class_get_type || !api.type_get_object) return nullptr;
        const auto* type = api.class_get_type(klass);
        Root managed(api, type ? api.type_get_object(type) : nullptr);
        if (!managed.Get()) return nullptr;
        int inactive = include_inactive, none = 0; void* args[] = {managed.Get(), &inactive, &none}; bool ok = false;
        Object* r = api.Call(find, nullptr, args, &ok);
        return ok ? r : nullptr;
    }
    bool Resolve() {
        if (!api.class_get_field_from_name || !api.field_get_value || !api.class_get_type || !api.type_get_object) { issue = "IL2CPP API"; return false; }
        auto method = [&](const char* ns, const char* type, const char* name, int n = 0) { return api.FindMethod(api.Find(ns, type), name, n); };
        chat_message_class = api.Find("", "ChatMessage"); chat_manager_class = api.Find("", "ChatManager");
        find = method("UnityEngine", "Object", "FindObjectsByType", 3);
        length = method("System", "Array", "get_Length"); item = method("System", "Array", "GetValueImpl", 1);
        alive = method("UnityEngine", "Object", "op_Implicit", 1); instance_id = method("UnityEngine", "Object", "GetInstanceID");
        active = method("UnityEngine", "Behaviour", "get_isActiveAndEnabled");
        get_transform = method("UnityEngine", "Component", "get_transform"); sibling = method("UnityEngine", "Transform", "GetSiblingIndex");
        text_get = method("TMPro", "TMP_Text", "get_text");
        auto* input = api.Find("TMPro", "TMP_InputField");
        input_text = api.FindMethod(input, "get_text", 0); input_set = api.FindMethod(input, "set_text", 1);
        input_focused = api.FindMethod(input, "get_isFocused", 0);
        send = api.FindMethod(chat_manager_class, "SendMessage", 0);
        queue = method("Photon.Pun", "PhotonNetwork", "get_IsMessageQueueRunning");
        room = method("Photon.Pun", "PhotonNetwork", "get_CurrentRoom");
        players = method("Photon.Pun", "PhotonNetwork", "get_PlayerListOthers"); // yerel oyuncu hariç
        player_class = api.Find("Photon.Realtime", "Player");
        own_nick = method("Photon.Pun", "PhotonNetwork", "get_NickName"); // isteğe bağlı: kendi adımız
        nick = api.FindMethod(player_class, "get_NickName", 0);
        if (chat_message_class) message_field = api.class_get_field_from_name(chat_message_class, "myMessage");
        if (chat_manager_class) {
            input_field = api.class_get_field_from_name(chat_manager_class, "inputField");
            allowed_field = api.class_get_field_from_name(chat_manager_class, "canSendMessage");
        }
        const std::pair<const void*, const char*> need[] = {{chat_message_class, "ChatMessage"}, {chat_manager_class, "ChatManager"},
            {find, "FindObjectsByType"}, {length, "Array.Length"}, {item, "Array.GetValueImpl"}, {alive, "Object.op_Implicit"},
            {instance_id, "GetInstanceID"}, {text_get, "TMP_Text.text"},
            {input_text, "TMP_InputField.text"}, {input_set, "TMP_InputField.set_text"}, {input_focused, "isFocused"}, {send, "SendMessage"},
            {message_field, "myMessage"}, {input_field, "inputField"}, {allowed_field, "canSendMessage"},
            {queue, "IsMessageQueueRunning"}, {room, "CurrentRoom"}, {players, "PlayerListOthers"}, {nick, "NickName"}};
        issue.clear();
        for (const auto& n : need) if (!n.first) issue += std::string(issue.empty() ? "" : ", ") + n.second;
        return issue.empty(); // get_transform/GetSiblingIndex isteğe bağlı: yoksa sıralama instance id ile
    }
    int Count(Object* array) { bool ok = false; auto* r = api.Call(length, array, nullptr, &ok); return ok && r ? *static_cast<int*>(api.object_unbox(r)) : -1; }
    Object* At(Object* array, int i) { void* a[] = {&i}; bool ok = false; Object* r = api.Call(item, array, a, &ok); return ok ? r : nullptr; }
    std::string ReadString(const Method* getter, Object* self, bool& ok) {
        ok = false; Object* r = api.Call(getter, self, nullptr, &ok);
        return ok && r ? api.Utf8(reinterpret_cast<String*>(r)) : std::string();
    }
    void Players() {
        if (own_nick) { bool ok = false; const std::string n = chat_bot_core::Trim(chat_bot_core::StripTags(ReadString(own_nick, nullptr, ok))); if (ok) own_name = n; }
        std::vector<std::string> names;
        Root list(api, api.Call(players));
        const int n = list.Get() ? Count(list.Get()) : -1;
        for (int i = 0; i < n && i < 32; ++i) {
            Root p(api, At(list.Get(), i)); if (!p.Get()) continue;
            bool ok = false;
            const std::string name = chat_bot_core::Trim(chat_bot_core::StripTags(ReadString(nick, p.Get(), ok)));
            if (ok && !name.empty() && std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
        }
        if (!list.Get() || n < 0) return; // okunamadı: eski liste ve bot durumu korunur
        snap.players = names;
        if (enabled && !all && !selected.empty()) {
            const bool any = std::any_of(selected.begin(), selected.end(), [&](const std::string& n) { return std::find(names.begin(), names.end(), n) != names.end(); });
            if (!any) { enabled = false; InvalidateReplies(); reminder.Reset(); Status(u8"AI | Seçilen oyuncuların hiçbiri odada değil; bot kapatıldı."); }
        }
    }
    void Status(const std::string& s) { snap.status = s; }
    // Mevcut tüm satırları gördük say: yalnız bundan sonra gelenlere cevap verilir.
    bool Scan(bool mark_only) {
        Root array(api, Array(chat_message_class, 1)); // kapalı sohbet penceresindeki satırlar da dahil: geçmiş yeniden etkinleşince 'yeni' sayılmasın
        const int n = array.Get() ? Count(array.Get()) : -1;
        if (n < 0 || n > 400) return false;
        struct New { int order; std::string plain; };
        std::vector<New> fresh; std::unordered_set<int> alive_ids;
        for (int i = 0; i < n; ++i) {
            Root o(api, At(array.Get(), i)); if (!o.Get() || !IsAlive(o.Get())) continue;
            bool ok = false; auto* id_box = api.Call(instance_id, o.Get(), nullptr, &ok);
            if (!ok || !id_box) continue;
            const int id = *static_cast<int*>(api.object_unbox(id_box));
            if (seen.count(id)) { alive_ids.insert(id); continue; }
            Object* tmp = nullptr; api.field_get_value(o.Get(), message_field, &tmp);
            Root label(api, tmp); if (!label.Get()) continue;
            const std::string text = ReadString(text_get, label.Get(), ok);
            if (!ok || chat_bot_core::Trim(text).empty()) continue; // metin henüz yazılmadı; sonra tekrar bak
            alive_ids.insert(id);
            if (mark_only) continue;
            int order = i; // yedek sıralama: dizi sırası (Unity runtime id değerleri negatif olabilir)
            Root tr(api, (get_transform && sibling) ? api.Call(get_transform, o.Get(), nullptr, &ok) : nullptr);
            if (tr.Get()) { auto* idx = api.Call(sibling, tr.Get(), nullptr, &ok); if (ok && idx) order = *static_cast<int*>(api.object_unbox(idx)); }
            fresh.push_back({order, text});
        }
        seen = std::move(alive_ids);
        if (fresh.size() > 2) { snap.last_in = u8"(geçmiş yenilendi; atlandı)"; return true; } // toplu belirme = eski satırlar, cevaplanmaz
        std::sort(fresh.begin(), fresh.end(), [](const New& a, const New& b) { return a.order < b.order; });
        for (const auto& f : fresh) {
            // Panelde yalnız ekran içi gösterim; sohbet metni loga yazılmaz.
            snap.last_in = chat_bot_core::PrefixScalars(chat_bot_core::Trim(chat_bot_core::StripTags(f.plain)), 60);
            if (!own_name.empty() && chat_bot_core::MatchSender(f.plain, own_name)) continue; // kendi satırımız
            const std::vector<std::string>& candidates = all ? snap.players : selected;
            for (const auto& name : candidates) {
                auto msg = chat_bot_core::MatchSender(f.plain, name);
                if (!msg) continue;
                const auto vote_command=chat_bot_core::Fold(chat_bot_core::Trim(*msg));
                if(vote_command=="/evet"||vote_command=="/hayir")break; // room moderation commands never reach the AI
                if (std::find(recent_sent.begin(), recent_sent.end(), *msg) != recent_sent.end()) break; // yankı koruması
                if(voice_ai_bridge::RoutesText()) {voice_ai_bridge::PushText({active_room,name,*msg});break;}
                // Gereksiz istek göndermemek için yerel süzgeçler (AI'ya gitmeden):
                if (message_filter&&chat_bot_core::IsNoise(*msg)) { snap.last_in = u8"(anlamsız/kahkaha; atlandı)"; break; }
                const bool mentions_me=chat_bot_core::MentionsBot(*msg,own_name);
                std::vector<std::string> others;
                for(const auto& p:snap.players)if(p!=name)others.push_back(p);
                if(message_filter&&!mentions_me&&chat_bot_core::AddressedToOther(*msg,others,own_name)){
                    talking[name]=now_+kSideTalkWindow;
                    for(const auto& o:others)if(chat_bot_core::AddressedToOther(*msg,{o},own_name))talking[o]=now_+kSideTalkWindow;
                    snap.last_in=u8"(başkasına yazılmış; atlandı)";break;
                }
                const auto eng=engaged.find(name);
                const bool in_conversation=eng!=engaged.end()&&now_<eng->second;
                if(chat_bot_core::DirectedProfanity(*msg,own_name,others,in_conversation)){
                    WarnProfanity(name);break;
                }
                // A complete hello/how-are-you phrase is handled locally even
                // without a name mention. A greeting plus a real question is not.
                const auto smalltalk=chat_bot_core::SmallTalkReply(*msg,name,own_name,mode);
                if(smalltalk){
                    const auto previous=last_smalltalk.find(name);
                    if(previous!=last_smalltalk.end()&&now_-previous->second<kSmallTalkGap)break;
                    last_smalltalk[name]=now_;
                    pending.push_back({name,*smalltalk,true});
                    while(pending.size()>6)pending.pop_front();
                    if(mentions_me)engaged[name]=now_+kConversationWindow;
                    break;
                }
                const auto previous=last_accept.find(name);
                if(previous!=last_accept.end()&&now_-previous->second<kPerSenderGap)break;
                // An explicit audience selection already addresses the bot.
                // Name gating is useful for the whole room, not a private
                // conversation with a player the user deliberately selected.
                if(all&&strict&&!mentions_me&&!in_conversation){snap.last_in=u8"(adımla gelmedi; atlandı)";break;}
                if(mentions_me)engaged[name]=now_+kConversationWindow;
                const auto side = talking.find(name);
                if (message_filter && all && !mentions_me && !in_conversation && side != talking.end() && now_ < side->second) { snap.last_in = u8"(başkalarıyla konuşuyor; atlandı)"; break; }
                last_accept[name] = now_;
                // Aynı kişinin bekleyen eski mesajı varsa yenisiyle değiştir (kuyruk şişmesin).
                bool merged = false;
                for (auto& q : pending) if (!q.canned && q.name == name) { q.text = chat_bot_core::PrefixScalars(*msg, 400); merged = true; break; }
                if (!merged) pending.push_back({name, chat_bot_core::PrefixScalars(*msg, 400), false});
                while (pending.size() > 6) pending.pop_front();
                break;
            }
        }
        return true;
    }
    Object* ChatOwner() {
        Root array(api, Array(chat_manager_class));
        const int n = array.Get() ? Count(array.Get()) : -1;
        if (n != 1) return nullptr; // belirsiz/yok: gönderme
        Object* o = At(array.Get(), 0);
        return IsAlive(o) ? o : nullptr;
    }
    // true: işlem bitti (gönderildi ya da vazgeçildi); false: sonra tekrar dene.
    bool TrySend(double now) {
        // Last defence: no control marker can be sent even if a future caller
        // accidentally puts one into the outbox.
        while(!outbox.empty()&&chat_bot_core::IsSkipReply(outbox.front(),request_sender))outbox.pop_front();
        if(outbox.empty()){DropOutbox();return false;}
        if(outbox_reminder&&(!enabled||!all||!strict||own_name!=reminder_name||active_room!=reminder_room)){
            DropOutbox();return false;
        }
        if (now - outbox_since > kOutboxTimeout) { Status(u8"AI | Cevap gönderilemedi (sohbet beklemede/odakta)."); DropOutbox(); return false; }
        Root owner(api, ChatOwner()); if (!owner.Get()) return false;
        bool allowed = false; api.field_get_value(owner.Get(), allowed_field, &allowed);
        if (!chat_send_gate::Available(allowed)) return false;
        Object* tmp = nullptr; api.field_get_value(owner.Get(), input_field, &tmp);
        Root input(api, tmp); if (!input.Get() || !IsAlive(input.Get())) return false;
        bool ok = false;
        auto* focused = api.Call(input_focused, input.Get(), nullptr, &ok);
        if (!ok || !focused || *static_cast<bool*>(api.object_unbox(focused))) return false; // kullanıcı yazıyor
        if (!chat_bot_core::Trim(ReadString(input_text, input.Get(), ok)).empty() || !ok) return false; // taslağı ezme
        Root text(api, reinterpret_cast<Object*>(api.string_new(outbox.front().c_str())));
        void* args[] = {text.Get()};
        api.Call(input_set, input.Get(), args, &ok);
        if (!ok) return false;
        api.Call(send, owner.Get(), nullptr, &ok);
        const std::string after = ReadString(input_text, input.Get(), ok);
        if (ok && after == outbox.front()) { // gönderilmedi (alan temizlenmedi): taslağı geri al
            Root empty(api, reinterpret_cast<Object*>(api.string_new(""))); void* a2[] = {empty.Get()};
            api.Call(input_set, input.Get(), a2, &ok);
            return false;
        }
        snap.last_out = chat_bot_core::PrefixScalars(outbox.front(), 60);
        recent_sent.push_back(outbox.front()); while (recent_sent.size() > 12) recent_sent.pop_front();
        Status(u8"AI | Cevap gönderildi.");
        next_request = std::max(next_request, now + kMinGap);
        outbox_since = now + kChunkGap; // sıradaki parça için zaman aşımı yeniden başlar
        gap_until = now + kChunkGap;
        return true;
    }
    void BeginEdit(Edit e) {
        editing = e;
        const std::string speech = e == Edit::Speech ? voice_ai_bridge::SpeechText() : std::string();
        keyboard_bridge::OpenNative(e == Edit::Persona ? custom.c_str() : e == Edit::Speech ? speech.c_str() : "", e == Edit::Key);
        Status(e == Edit::Key ? u8"AI | API anahtarını yaz, Enter'a bas." : e == Edit::Speech ? u8"AI | Seslendirilecek mesajı yaz, Enter'a bas." : u8"AI | Kişiliği yaz, Enter'a bas.");
    }
    void PollEdit() {
        if (editing == Edit::None) return;
        std::array<char, text_buffer::kCapacity> buffer{}; bool submit = false, visible = false;
        if (!keyboard_bridge::PollNative(buffer.data(), buffer.size(), submit, visible)) { editing = Edit::None; Status(u8"AI | Düzenleme iptal."); return; }
        if (submit) {
            const std::string value = chat_bot_core::Trim(buffer.data());
            if (editing == Edit::Key) { key = value; SaveConfig(); Status(key.empty() ? u8"AI | Anahtar silindi." : u8"AI | Anahtar kaydedildi (cihazda kalıcı)."); }
            else if (editing == Edit::Speech) { Status(voice_ai_bridge::Speak(chat_bot_core::PrefixScalars(value, 300)) ? u8"AI | Mesaj seslendiriliyor." : u8"AI | Mesaj kaydedildi; Sesli AI açıkken seslendirilir."); }
            else { custom = value; mode = value.empty() ? 0 : chat_bot_core::kPersonaCount; SaveConfig(); InvalidateReplies(); history.clear(); QueueStatus(chat_bot_fx::Event::Profile,chat_bot_core::PersonaName(mode));Status(u8"AI | Özel kişilik kaydedildi, hafıza sıfırlandı."); }
            editing = Edit::None; keyboard_bridge::CloseNative();
        } else if (!visible) { editing = Edit::None; keyboard_bridge::CloseNative(); Status(u8"AI | Düzenleme iptal."); }
    }
    void Commands() {
        const auto c = chat_bot_bridge::Take();
        if (!c.Any()) return;
        if (c.select) {
            const auto it = std::find(selected.begin(), selected.end(), c.name);
            if (it == selected.end()) selected.push_back(c.name); else selected.erase(it);
            InvalidateReplies(); baseline = false; if (enabled) Status(u8"AI | Kişi seçimi değişti.");
        }
        if (c.toggle_all) { all = !all; SaveConfig(); InvalidateReplies(); baseline = false; Status(all ? u8"AI | Odadaki herkese cevap verilecek." : u8"AI | Yalnız seçili kişilere cevap verilecek."); }
        if(c.toggle_filter){message_filter=!message_filter;SaveConfig();InvalidateReplies();baseline=false;Status(message_filter?u8"AI | Selam/kahkaha mesaj filtresi açık.":u8"AI | Mesaj filtresi kapalı; kahkaha ve diğer mesajlar AI'ye iletilir. Selam cevabı sabittir.");}
        if (c.toggle_strict) { strict = !strict; SaveConfig(); InvalidateReplies(); reminder.Reset(); Status(strict ? u8"AI | Yalnız adımla yazılanlara cevap verilecek." : u8"AI | Tüm mesajlara cevap verilecek."); }
        if (c.set_words) { words = chat_bot_core::ClampWords(c.words); SaveConfig(); }
        if (c.set_mode) { const int previous=mode;mode = c.mode; if (mode == chat_bot_core::kPersonaCount && custom.empty()) mode = 0; SaveConfig(); InvalidateReplies(); history.clear(); Status(std::string(u8"AI | Kişilik: ") + chat_bot_core::PersonaName(mode) + u8"."); if(mode!=previous)QueueStatus(chat_bot_fx::Event::Profile,chat_bot_core::PersonaName(mode)); }
        if (c.toggle) {
            if (enabled) { enabled = false; InvalidateReplies(); reminder.Reset(); Status(u8"AI | Bot kapalı."); QueueStatus(chat_bot_fx::Event::Stop); }
            else if (key.empty()) Status(u8"AI | Önce API anahtarını gir.");
            else if (!all && selected.empty()) Status(u8"AI | Önce kişi seç ya da \"Herkes\"i aç.");
            else { enabled = true; baseline = false; InvalidateReplies(); reminder.Reset(); Status(u8"AI | Bot açık; yeni mesajlar bekleniyor."); status_model=engine.Model();QueueStatus(chat_bot_fx::Event::Start,status_model); }
        }
        if (c.reset_memory) { InvalidateReplies(); history.clear(); Status(u8"AI | Hafıza sıfırlandı."); }
        if (c.refresh) next_players = 0;
        if (c.edit != Edit::None) BeginEdit(c.edit);
    }
    void Publish() {
        snap.enabled = enabled; snap.selected = selected; snap.all = all; snap.strict = strict; snap.message_filter=message_filter; snap.words = words; snap.mode = mode; snap.has_key = !key.empty(); snap.busy = engine.Busy();
        snap.model = engine.Model(); snap.persona = chat_bot_core::PrefixScalars(custom, 80); snap.chat_ready = true;
        chat_bot_bridge::Publish(snap);
    }
public:
    explicit Controller(const Api& a) : api(a), engine(&chat_bot_http::Post) {}
    void Tick(double now) {
        if (disabled) return;
        if (start == 0) { start = now + 4; next = start; }
        if (now < next) { if(initialized)PumpStatus(now);return; }
        next = now + 0.25; now_ = now;
        if (!initialized) {
            initialized = true;
            LoadConfig();
            if (!Resolve()) { disabled = true; snap.chat_ready = false; snap.status = u8"AI | Oyun sınıfı bulunamadı: " + issue; chat_bot_bridge::Publish(snap); return; }
        }
        bool ok = false;
        auto* running = api.Call(queue, nullptr, nullptr, &ok);
        if (!ok || !running || !*static_cast<bool*>(api.object_unbox(running))) { return; } // sahne yükleniyor
        Commands(); PollEdit();
        voice_ai_bridge::Configure({key,System(),own_name,all,strict,selected});
        const bool route_voice=voice_ai_bridge::RoutesText();
        if(route_voice!=voice_routed){InvalidateReplies();baseline=false;voice_routed=route_voice;}

        Root current_room(api, api.Call(room));
        if(!current_room.Get()){
            ClearStatus();
            snap.players.clear();reminder.Reset();active_room=0;
            if(enabled){enabled=false;InvalidateReplies();Status(u8"AI | Odadan çıkıldı; bot kapatıldı.");}
            else DropOutbox();
            Publish();return;
        }
        // Numeric identity only; the object is rooted for this tick and this
        // saved value is never dereferenced or sent across threads.
        const auto room_id=reinterpret_cast<uintptr_t>(current_room.Get());
        if(active_room&&active_room!=room_id){
            ClearStatus();
            InvalidateReplies();reminder.Reset();baseline=false;seen.clear();
            engaged.clear();talking.clear();last_accept.clear();last_smalltalk.clear();last_warning.clear();next_warning=0;history.clear();
        }
        active_room=room_id;
        if (now >= next_players) { next_players = now + 1.0; Players(); }
        reminder.Update(now,enabled&&all,strict,active_room,own_name,!snap.players.empty());
        if(outbox_reminder&&(!enabled||!all||!strict||reminder_name!=own_name||reminder_room!=active_room||snap.players.empty()))DropOutbox();
        if (enabled||route_voice) {
            if (!baseline) { if (Scan(true)) baseline = true; }
            else Scan(false);
            chat_bot_engine::Result r;
            if (engine.Poll(r)) {
                if(request_generation!=reply_generation){Status(u8"AI | Eski oturumun yanıtı atlandı.");}
                else if (r.ok) {
                    if(r.model!=status_model){status_model=r.model;QueueStatus(chat_bot_fx::Event::Model,status_model);}
                    const auto whole=chat_bot_core::PrepareReply(r.text,static_cast<size_t>(words),request_sender);
                    if(whole.empty()){
                        if(!history.empty()&&!history.back().model)history.pop_back();
                        Status(u8"AI | Yanıt gerekmiyor; sessizce atlandı.");next_request=now+1.0;
                    }else{
                        if(outbox_reminder)DropOutbox();
                        for(auto& piece:chat_bot_core::SplitChunks(whole,kChunkScalars,MaxChunks()))outbox.push_back(piece);
                        history.push_back({true,whole});chat_bot_core::TrimHistory(history,12);outbox_since=now;
                        Status(u8"AI | Cevap hazır ("+r.model+").");
                    }
                } else if (r.empty) { Status(u8"AI | Model cevap vermedi (süzgeç/boş); mesaj atlandı."); next_request = now + kMinGap; }
                else { Status(u8"AI | Tüm modeller hata verdi (HTTP " + std::to_string(r.code) + u8"). Anahtarı/ağı kontrol et."); next_request = now + 8; }
            }
            if(outbox_reminder&&(!pending.empty()||engine.Busy()))DropOutbox();
            if (status_lines.empty() && !outbox.empty() && now >= gap_until && TrySend(now)) {
                outbox.pop_front();if(warning_chunks)--warning_chunks;
                if(outbox.empty())outbox_reminder=false;
            }
            else if (!outbox.empty() && now - outbox_since > kOutboxTimeout) { DropOutbox(); } // TrySend zaten durum yazdı
            if (status_lines.empty() && outbox.empty() && !pending.empty() && now >= next_request) {
                if (pending.front().canned) { // selam: AI'ya gitmeden hazır cevap
                    outbox.push_back(pending.front().text); pending.pop_front(); outbox_since = now; Status(u8"AI | Selam cevabı hazır.");
                } else if(!engine.Busy()&&!key.empty()) {
                    request_sender=pending.front().name;request_generation=reply_generation;
                    history.push_back({false, pending.front().name + ": " + pending.front().text}); pending.pop_front();
                    chat_bot_core::TrimHistory(history, 16);
                    if (engine.Submit(System(), history, key)) Status(u8"AI | Düşünüyor...");
                }
            }
            // No API call: the hint goes through exactly the same cooldown,
            // input-focus/draft and timeout checks as an ordinary chat reply.
            if(enabled&&!route_voice&&baseline&&reminder.Due(now)&&outbox.empty()&&pending.empty()&&!engine.Busy()&&now>=gap_until&&now>=next_request){
                const auto text=chat_bot_core::NameReminderText(own_name);
                if(!text.empty()){
                    outbox.push_back(text);outbox_reminder=true;reminder_name=own_name;reminder_room=active_room;
                    outbox_since=now;reminder.Queued(now);
                }
            }
        }
        PumpStatus(now); // also runs after the bot has been switched OFF
        Publish();
    }
};
}
void Pump(const Api& api) {
    static Controller* controller = nullptr; static bool stopped = false;
    if (stopped) return;
    try {
        if (!controller) controller = new Controller(api);
        controller->Tick(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
    } catch (...) {
        stopped = true;
        chat_bot_bridge::Snapshot s; s.status = u8"AI | Modül durdu (hata). Klavye kullanılabilir."; chat_bot_bridge::Publish(s);
        BOT_LOG("chat_bot: native exception; module stopped");
    }
}
}
