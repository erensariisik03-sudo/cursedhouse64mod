#pragma once
// Reklam engeli arka planda sürekli açıktır (menü yok). Yalnız banner ve geçiş (interstitial) reklamları;
// ödüllü reklamlara dokunulmaz ve ödül asla taklit edilmez.
#include <atomic>
namespace ad_block {
inline std::atomic<bool> g_enabled{true};
inline std::atomic<int> g_hooked{0};          // kaç kanca kuruldu (log için)
inline std::atomic<unsigned> g_blocked{0};    // kaç reklam çağrısı engellendi
}
