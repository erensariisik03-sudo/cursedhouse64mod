#include "auto_join.h"
#include "auto_join_bridge.h"
#include "dobby.h"
#include <android/log.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#define AJ_LOG(...) __android_log_print(ANDROID_LOG_INFO, "multiplayermod", __VA_ARGS__)
namespace auto_join {
namespace {
using namespace il2cpp;
constexpr short kGameFull = 32765;              // Photon ErrorCode.GameFull
constexpr double kOfferSeconds = 45.0;          // hata sonrası düğmenin görünme süresi
constexpr double kRetryGap = 2.0;               // iki deneme arası
constexpr double kMaxSeconds = 600.0;           // en çok 10 dakika dene
const Api* g_api = nullptr;
std::mutex g_mutex;
std::string g_room;                             // oyunun en son JoinRoom(name) çağrısı
std::atomic<double> g_fail_at{-1e9};
std::atomic<bool> g_active{false};
bool (*g_join_orig)(String*, void*, const void*) = nullptr;
void (*g_fail_orig[3])(void*, short, String*, const void*) = {nullptr, nullptr, nullptr};
double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
bool JoinRoomHook(String* name, void* expected, const void* method) {
    if (g_api && name) { std::string text = g_api->Utf8(name); if (!text.empty()) { std::lock_guard<std::mutex> l(g_mutex); g_room = text; } }
    return g_join_orig ? g_join_orig(name, expected, method) : false;
}
template<int N> void FailHook(void* self, short code, String* message, const void* method) {
    if (code == kGameFull) g_fail_at = Now();
    if (g_active.load()) {
        if (code == kGameFull) return;          // otomatik denerken oyunun "Giriş başarısız" penceresi her denemede açılmasın
        g_active = false;                       // başka bir hata: döngüyü bırak, oyun kendi akışını yürütsün
    }
    if (g_fail_orig[N]) g_fail_orig[N](self, code, message, method);
}
template<class T> bool HookMethod(const Api& api, Class* klass, const char* name, int args, void* replacement, T& original, const char* label, bool own) {
    const Method* method = klass ? api.FindMethod(klass, name, args) : nullptr;
    if (!method) { AJ_LOG("v25 autojoin: %s not found", label); return false; }
    if (own && api.method_get_class && api.method_get_class(method) != klass) { AJ_LOG("v25 autojoin: %s is inherited; skipped", label); return false; }
    void* address = Entry(method);
    if (!address || DobbyHook(address, replacement, reinterpret_cast<void**>(&original)) != 0 || !original) { original = nullptr; AJ_LOG("v25 autojoin: %s hook failed", label); return false; }
    AJ_LOG("v25 autojoin: %s hooked", label); return true;
}
struct Meta {
    bool resolved = false, ok = false;
    const Method *in_room = nullptr, *connected = nullptr, *join = nullptr;
} g_meta;
bool Resolve(const Api& api) {
    if (g_meta.resolved) return g_meta.ok;
    g_meta.resolved = true;
    Class* network = api.Find("Photon.Pun", "PhotonNetwork");
    if (!network) return false;
    g_meta.in_room = api.FindMethod(network, "get_InRoom", 0);
    g_meta.connected = api.FindMethod(network, "get_IsConnectedAndReady", 0);
    g_meta.join = api.FindMethod(network, "JoinRoom", 2);
    g_meta.ok = g_meta.in_room && g_meta.connected && g_meta.join && api.string_new;
    if (!g_meta.ok) AJ_LOG("v25 autojoin: PhotonNetwork methods missing; offer disabled");
    return g_meta.ok;
}
}
void Install(const Api& api) {
    g_api = &api;
    Class* network = api.Find("Photon.Pun", "PhotonNetwork");
    HookMethod(api, network, "JoinRoom", 2, reinterpret_cast<void*>(JoinRoomHook), g_join_orig, "PhotonNetwork.JoinRoom", true);
    HookMethod(api, api.Find("", "launcher"), "OnJoinRoomFailed", 2, reinterpret_cast<void*>(FailHook<0>), g_fail_orig[0], "launcher.OnJoinRoomFailed", true);
    HookMethod(api, api.Find("", "PhotonRoomsManager"), "OnJoinRoomFailed", 2, reinterpret_cast<void*>(FailHook<1>), g_fail_orig[1], "PhotonRoomsManager.OnJoinRoomFailed", true);
    HookMethod(api, api.Find("", "RoomInviteJoiner"), "OnJoinRoomFailed", 2, reinterpret_cast<void*>(FailHook<2>), g_fail_orig[2], "RoomInviteJoiner.OnJoinRoomFailed", true);
}
void Pump(const Api& api) {
    static double next_try = 0, started = 0; static int attempts = 0;
    if (!g_api || !Resolve(api)) return;
    const auto in_room = [&]() { auto* boxed = api.Call(g_meta.in_room); return boxed && *static_cast<bool*>(api.object_unbox(boxed)); };
    const auto ready = [&]() { auto* boxed = api.Call(g_meta.connected); return boxed && *static_cast<bool*>(api.object_unbox(boxed)); };
    std::string room; { std::lock_guard<std::mutex> l(g_mutex); room = g_room; }
    const double now = Now();
    const auto cmd = auto_join_bridge::Take();
    std::string status;
    if (cmd.cancel) { g_active = false; status = u8"Otomatik bağlanma durduruldu."; }
    if (cmd.start && !room.empty() && !g_active.load()) { g_active = true; started = now; attempts = 0; next_try = 0; status = u8"Oda boşalınca bağlanılacak..."; AJ_LOG("v25 autojoin: started"); }
    if (g_active.load()) {
        if (in_room()) { g_active = false; status = u8"Odaya bağlanıldı."; AJ_LOG("v25 autojoin: joined"); }
        else if (now - started > kMaxSeconds) { g_active = false; status = u8"10 dakika denendi, oda boşalmadı; durduruldu."; }
        else if (now >= next_try && ready()) {
            next_try = now + kRetryGap; ++attempts;
            if (String* name = api.string_new(room.c_str())) {
                const Handle root = api.gchandle_new(reinterpret_cast<Object*>(name), false);
                void* args[] = {name, nullptr}; bool ok = false;
                api.Call(g_meta.join, nullptr, args, &ok);
                if (root) api.gchandle_free(root);
            }
            status = u8"Deneniyor... oda hâlâ doluysa tekrar denenecek.";
        }
    }
    auto_join_bridge::Snapshot snap;
    snap.active = g_active.load(); snap.attempts = attempts; snap.room = room;
    snap.offer = !snap.active && !room.empty() && now - g_fail_at.load() < kOfferSeconds && !in_room();
    snap.status = status.empty() ? auto_join_bridge::Read().status : status;
    auto_join_bridge::Publish(snap);
}
}
