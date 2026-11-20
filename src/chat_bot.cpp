#include "chat_bot.h"
#include "chat_bot_bridge.h"
#include "chat_bot_core.h"
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
constexpr size_t kReplyScalars = 600;  // toplam cevap sınırı
constexpr size_t kChunkScalars = 200;  // tek sohbet satırı: bundan uzunsa cümle sonunda bölünür (satırlar üst üste binmesin)
constexpr size_t kMaxChunks = 2;
constexpr double kChunkGap = 1.2;
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
    std::vector<std::string> selected; bool all = false, strict = true; int mode = 0, words = chat_bot_core::kMinWords;
    struct Pending { std::string name, text; bool canned = false; };
    std::unordered_map<std::string, double> last_accept, engaged, talking; // talking: başkalarıyla konuşan kişiler // engaged: bize yazan kişinin 'devam eden konuşma' süresi
    double now_ = 0;
    std::deque<Pending> pending;
    std::deque<std::string> outbox, recent_sent; // gönderilecek parçalar; kendi yolladıklarımız (yankı koruması)
    bool enabled = false, baseline = false;
    Edit editing = Edit::None;
    std::string issue, own_name;

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
        key = c.key; custom = c.custom; mode = c.mode; all = c.all; strict = c.strict; words = c.words;
        if (!key.empty()) Status(u8"AI | Kayıtlı anahtar yüklendi.");
    }
    void SaveConfig() {
        const std::string path = ConfigPath(); if (path.empty()) { BOT_LOG("chat_bot: config path unavailable"); return; }
        chat_bot_core::Config c; c.key = key; c.custom = custom; c.mode = mode; c.all = all; c.strict = strict; c.words = words;
        const std::string data = chat_bot_core::SerializeConfig(c);
        std::ofstream out(path, std::ios::binary | std::ios::trunc); // uygulamanın özel dizini; anahtar loglanmaz
        if (out) { out << data; out.close(); chmod(path.c_str(), 0600); }
    }
    bool Selected(const std::string& n) const { return std::find(selected.begin(), selected.end(), n) != selected.end(); }
    std::string System() const { return chat_bot_core::BuildSystem(mode, custom, static_cast<size_t>(words)); }
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
            if (!any) { enabled = false; pending.clear(); Status(u8"AI | Seçilen oyuncuların hiçbiri odada değil; bot kapatıldı."); }
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
                if (std::find(recent_sent.begin(), recent_sent.end(), *msg) != recent_sent.end()) break; // yankı koruması
                // Gereksiz istek göndermemek için yerel süzgeçler (AI'ya gitmeden):
                if (chat_bot_core::IsNoise(*msg)) { snap.last_in = u8"(anlamsız/kahkaha; atlandı)"; break; }
                auto& last = last_accept[name];
                if (now_ - last < kPerSenderGap) break;                       // aynı kişi art arda yazıyorsa sıklığı sınırla
                if (chat_bot_core::IsSalam(*msg)) {
                    last = now_;
                    pending.push_back({name, name + u8", aleyküm selam ve rahmetullahi ve berekatuhu.", true});
                    while (pending.size() > 6) pending.pop_front();
                    break;
                }
                const bool mentions_me = !own_name.empty() && chat_bot_core::MentionsName(*msg, own_name);
                const auto eng = engaged.find(name);
                const bool in_conversation = eng != engaged.end() && now_ < eng->second; // az önce bize yazdı/cevap verdik
                if (strict && !own_name.empty() && !mentions_me && !in_conversation) { snap.last_in = u8"(adımla gelmedi; atlandı)"; break; }
                if (mentions_me) engaged[name] = now_ + kConversationWindow;
                std::vector<std::string> others;
                for (const auto& p : snap.players) if (p != name) others.push_back(p);
                if (!mentions_me && chat_bot_core::AddressedToOther(*msg, others, own_name)) {
                    // İki oyuncu birbirine yazıyor: ikisini de bir süre "başkalarıyla konuşuyor" say.
                    talking[name] = now_ + kSideTalkWindow;
                    for (const auto& o : others) if (chat_bot_core::AddressedToOther(*msg, {o}, own_name)) talking[o] = now_ + kSideTalkWindow;
                    snap.last_in = u8"(başkasına yazılmış; atlandı)"; break;
                }
                const auto side = talking.find(name);
                if (!mentions_me && !in_conversation && side != talking.end() && now_ < side->second) { snap.last_in = u8"(başkalarıyla konuşuyor; atlandı)"; break; }
                last = now_;
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
        if (now - outbox_since > kOutboxTimeout) { Status(u8"AI | Cevap gönderilemedi (sohbet beklemede/odakta)."); outbox.clear(); return false; }
        Root owner(api, ChatOwner()); if (!owner.Get()) return false;
        bool allowed = false; api.field_get_value(owner.Get(), allowed_field, &allowed);
        if (!allowed) return false; // oyunun kendi bekleme süresi
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
        keyboard_bridge::OpenNative(e == Edit::Persona ? custom.c_str() : "", e == Edit::Key);
        Status(e == Edit::Key ? u8"AI | API anahtarını yaz, Enter'a bas." : u8"AI | Kişiliği yaz, Enter'a bas.");
    }
    void PollEdit() {
        if (editing == Edit::None) return;
        std::array<char, text_buffer::kCapacity> buffer{}; bool submit = false, visible = false;
        if (!keyboard_bridge::PollNative(buffer.data(), buffer.size(), submit, visible)) { editing = Edit::None; Status(u8"AI | Düzenleme iptal."); return; }
        if (submit) {
            const std::string value = chat_bot_core::Trim(buffer.data());
            if (editing == Edit::Key) { key = value; SaveConfig(); Status(key.empty() ? u8"AI | Anahtar silindi." : u8"AI | Anahtar kaydedildi (cihazda kalıcı)."); }
            else { custom = value; mode = value.empty() ? 0 : chat_bot_core::kPersonaCount; SaveConfig(); history.clear(); Status(u8"AI | Özel kişilik kaydedildi, hafıza sıfırlandı."); }
            editing = Edit::None; keyboard_bridge::CloseNative();
        } else if (!visible) { editing = Edit::None; keyboard_bridge::CloseNative(); Status(u8"AI | Düzenleme iptal."); }
    }
    void Commands() {
        const auto c = chat_bot_bridge::Take();
        if (!c.Any()) return;
        if (c.select) {
            const auto it = std::find(selected.begin(), selected.end(), c.name);
            if (it == selected.end()) selected.push_back(c.name); else selected.erase(it);
            pending.clear(); baseline = false; if (enabled) Status(u8"AI | Kişi seçimi değişti.");
        }
        if (c.toggle_all) { all = !all; SaveConfig(); pending.clear(); baseline = false; Status(all ? u8"AI | Odadaki herkese cevap verilecek." : u8"AI | Yalnız seçili kişilere cevap verilecek."); }
        if (c.toggle_strict) { strict = !strict; SaveConfig(); pending.clear(); Status(strict ? u8"AI | Yalnız adımla yazılanlara cevap verilecek." : u8"AI | Tüm mesajlara cevap verilecek."); }
        if (c.set_words) { words = chat_bot_core::ClampWords(c.words); SaveConfig(); }
        if (c.set_mode) { mode = c.mode; if (mode == chat_bot_core::kPersonaCount && custom.empty()) mode = 0; SaveConfig(); history.clear(); Status(std::string(u8"AI | Kişilik: ") + chat_bot_core::PersonaName(mode) + u8"."); }
        if (c.toggle) {
            if (enabled) { enabled = false; pending.clear(); outbox.clear(); Status(u8"AI | Bot kapalı."); }
            else if (key.empty()) Status(u8"AI | Önce API anahtarını gir.");
            else if (!all && selected.empty()) Status(u8"AI | Önce kişi seç ya da \"Herkes\"i aç.");
            else { enabled = true; baseline = false; pending.clear(); outbox.clear(); Status(u8"AI | Bot açık; yeni mesajlar bekleniyor."); }
        }
        if (c.reset_memory) { history.clear(); Status(u8"AI | Hafıza sıfırlandı."); }
        if (c.refresh) next_players = 0;
        if (c.edit != Edit::None) BeginEdit(c.edit);
    }
    void Publish() {
        snap.enabled = enabled; snap.selected = selected; snap.all = all; snap.strict = strict; snap.words = words; snap.mode = mode; snap.has_key = !key.empty(); snap.busy = engine.Busy();
        snap.model = engine.Model(); snap.persona = chat_bot_core::PrefixScalars(custom, 80); snap.chat_ready = true;
        chat_bot_bridge::Publish(snap);
    }
public:
    explicit Controller(const Api& a) : api(a), engine(&chat_bot_http::Post) {}
    void Tick(double now) {
        if (disabled) return;
        if (start == 0) { start = now + 4; next = start; }
        if (now < next) return;
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
        Root current_room(api, api.Call(room));
        if (!current_room.Get()) { snap.players.clear(); if (enabled) { enabled = false; pending.clear(); Status(u8"AI | Odadan çıkıldı; bot kapatıldı."); } Publish(); return; }
        if (now >= next_players) { next_players = now + 1.0; Players(); }
        if (enabled) {
            if (!baseline) { if (Scan(true)) baseline = true; }
            else Scan(false);
            chat_bot_engine::Result r;
            if (engine.Poll(r)) {
                if (r.ok) {
                    if (chat_bot_core::IsSkipReply(chat_bot_core::CleanReply(r.text, kReplyScalars))) {
                        if (!history.empty() && !history.back().model) history.pop_back(); // cevaplanmayan mesajı hafızadan çıkar
                        Status(u8"AI | Mesaj bana değil; atlandı."); next_request = now + 1.0; Publish(); return;
                    }
                    const std::string whole = chat_bot_core::LimitWords(chat_bot_core::CleanReply(r.text, static_cast<size_t>(words) * 9), static_cast<size_t>(words));
                    for (auto& piece : chat_bot_core::SplitChunks(whole, kChunkScalars, MaxChunks())) outbox.push_back(piece);
                    if (outbox.empty()) Status(u8"AI | Cevap boş kaldı.");
                    else { history.push_back({true, whole}); chat_bot_core::TrimHistory(history, 12); outbox_since = now; Status(u8"AI | Cevap hazır (" + r.model + ")."); }
                } else if (r.empty) { Status(u8"AI | Model cevap vermedi (süzgeç/boş); mesaj atlandı."); next_request = now + kMinGap; }
                else { Status(u8"AI | Tüm modeller hata verdi (HTTP " + std::to_string(r.code) + u8"). Anahtarı/ağı kontrol et."); next_request = now + 8; }
            }
            if (!outbox.empty() && now >= gap_until && TrySend(now)) outbox.pop_front();
            else if (!outbox.empty() && now - outbox_since > kOutboxTimeout) { outbox.clear(); } // TrySend zaten durum yazdı
            if (!engine.Busy() && outbox.empty() && !pending.empty() && now >= next_request && !key.empty()) {
                if (pending.front().canned) { // selam: AI'ya gitmeden hazır cevap
                    outbox.push_back(pending.front().text); pending.pop_front(); outbox_since = now; Status(u8"AI | Selam cevabı hazır.");
                } else {
                    history.push_back({false, pending.front().name + ": " + pending.front().text}); pending.pop_front();
                    chat_bot_core::TrimHistory(history, 16);
                    if (engine.Submit(System(), history, key)) Status(u8"AI | Düşünüyor...");
                }
            }
        }
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
