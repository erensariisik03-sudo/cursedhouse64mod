#include "ad_block.h"
#include "ad_block_state.h"
#include "dobby.h"
#include <android/log.h>
#define AD_LOG(...) __android_log_print(ANDROID_LOG_INFO, "multiplayermod", __VA_ARGS__)
namespace ad_block {
namespace {
using namespace il2cpp;
bool (*g_can_show)(void*, const void*) = nullptr;
void (*g_show)(void*, const void*) = nullptr;
void (*g_banner)(void*, const void*) = nullptr;
// Engel açıkken: oyunun "reklam yok" yolunu kullan (CanShowAd=false) ve Show/LoadBanner çağrılarını yut.
// Kapalıyken orijinale devret.
bool CanShow(void* self, const void* method) { if (g_enabled.load()) { ++g_blocked; return false; } return g_can_show ? g_can_show(self, method) : false; }
void Show(void* self, const void* method) { if (g_enabled.load()) { ++g_blocked; return; } if (g_show) g_show(self, method); }
void Banner(void* self, const void* method) { if (g_enabled.load()) { ++g_blocked; return; } if (g_banner) g_banner(self, method); }
template<class T> void HookOne(const Api& api, Class* klass, const char* name, void* replacement, T& original, const char* label) {
    const Method* method = klass ? api.FindMethod(klass, name, 0) : nullptr;
    void* address = Entry(method);
    if (!address) { AD_LOG("v24 ads: %s not found; skipped", label); return; }
    if (DobbyHook(address, replacement, reinterpret_cast<void**>(&original)) != 0 || !original) { original = nullptr; AD_LOG("v24 ads: %s hook failed; skipped", label); return; }
    ++g_hooked; AD_LOG("v24 ads: %s hooked", label);
}
}
void Install(const Api& api) {
    Class* interstitial = api.Find("GoogleMobileAds.Api", "InterstitialAd");
    Class* manager = api.Find("", "AdmobAdsManager");
    HookOne(api, interstitial, "CanShowAd", reinterpret_cast<void*>(CanShow), g_can_show, "InterstitialAd.CanShowAd");
    HookOne(api, interstitial, "Show", reinterpret_cast<void*>(Show), g_show, "InterstitialAd.Show");
    HookOne(api, manager, "LoadBannerAd", reinterpret_cast<void*>(Banner), g_banner, "AdmobAdsManager.LoadBannerAd");
}
}
