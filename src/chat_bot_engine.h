#pragma once
// Gemini isteği + model yedekleme. Ağ işi ayrı thread'de yapılır; Unity thread'i
// yalnızca Submit/Poll çağırır (bloklanmaz). HTTP işlevi enjekte edilir (host testi).
#include "chat_bot_core.h"
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace chat_bot_engine {
using HttpFn = bool (*)(const std::string& url, const std::string& key, const std::string& body, int& code, std::string& response);
inline constexpr std::array<const char*, 6> kModels = {
    // Öncelik sırası: en hızlı / en yüksek kotalı "lite" modeller önce, daha yavaş büyük modeller yedek.
    "gemini-3.1-flash-lite-preview", "gemini-3.1-flash-lite", "gemini-2.5-flash-lite", "gemini-flash-lite-latest",
    "gemini-2.5-flash", "gemini-3-flash-preview"};
struct Result {
    bool ok = false, empty = false, all_failed = false;
    int code = 0;           // son HTTP durumu (0 = bağlantı yok)
    std::string text, model; // ok ise cevap ve kullanılan model
};
class Engine {
    struct Shared {
        std::mutex mutex; bool done = false; Result result;
    };
    // Her istek öncelik sırasının başından denenir; kota/hata veren model bir süre dinlendirilir (atlanır).
    struct Pool {
        std::atomic<size_t> last{0};
        std::array<std::atomic<long long>, kModels.size()> cool_until_ms{};
    };
    HttpFn http_;
    std::shared_ptr<Shared> job_;
    std::shared_ptr<Pool> pool_ = std::make_shared<Pool>();
    static long long NowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
    static long long CooldownMs(bool sent, int code) {
        if (!sent) return 15000;
        if (code == 429) return 60000;           // dakika/gün kotası: bir dakika dinlendir
        if (code == 404) return 600000;          // model adı yok/kapalı: uzun süre atla
        if (code == 400 || code == 401 || code == 403) return 30000;
        return 15000;
    }
public:
    explicit Engine(HttpFn http) : http_(http) {}
    bool Busy() const { return job_ != nullptr; }
    std::string Model() const { return kModels[pool_->last.load() % kModels.size()]; } // en son başarılı model
    void Reset() { pool_->last = 0; for (auto& c : pool_->cool_until_ms) c = 0; } // testler için
    bool Submit(const std::string& system, const std::vector<chat_bot_core::Turn>& history, const std::string& key) {
        if (job_) return false;
        auto shared = std::make_shared<Shared>();
        job_ = shared;
        const HttpFn http = http_;
        auto pool = pool_;
        std::thread([=] { Run(http, shared, pool, system, history, key); }).detach();
        return true;
    }
    bool Poll(Result& out) {
        if (!job_) return false;
        {
            std::lock_guard<std::mutex> l(job_->mutex);
            if (!job_->done) return false;
            out = job_->result;
        }
        job_.reset(); // yalnız çağıran thread job_ değiştirir; işçi kendi shared_ptr'ini tutar
        return true;
    }
    static void Run(HttpFn http, std::shared_ptr<Shared> shared, std::shared_ptr<Pool> pool,
                    std::string system, std::vector<chat_bot_core::Turn> history, std::string key) {
        Result r;
        bool tried_any = false;
        for (int pass = 0; pass < 2 && !tried_any; ++pass) { // 1. tur: dinlenenleri atla; hepsi dinleniyorsa 2. tur: yine de dene
            for (size_t k = 0; k < kModels.size() && !r.ok && !r.empty; ++k) {
                const size_t at = k;
                if (pass == 0 && pool->cool_until_ms[at].load() > NowMs()) continue;
                tried_any = true;
                const std::string model = kModels[at];
                const std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent";
                const std::string body = chat_bot_core::BuildRequest(system, history, model);
                for (int attempt = 0; attempt < 2; ++attempt) {
                    int code = 0; std::string response;
                    const bool sent = http(url, key, body, code, response);
                    r.code = sent ? code : 0;
                    if (sent && code == 200) {
                        auto text = chat_bot_core::ExtractText(response);
                        if (text && !chat_bot_core::Trim(*text).empty()) { r.ok = true; r.text = *text; r.model = model; pool->last = at; }
                        else r.empty = true; // güvenlik süzgeci / boş cevap: model hatası sayılmaz
                        break;
                    }
                    const bool transient = !sent || code == 500 || code == 502 || code == 503 || code == 504;
                    if (!transient || attempt == 1) pool->cool_until_ms[at] = NowMs() + CooldownMs(sent, code);
                    if (!transient) break; // kalıcı istemci hatası: aynı modeli tekrar deneme
                }
            }
        }
        if (!r.ok && !r.empty) r.all_failed = true;
        std::lock_guard<std::mutex> l(shared->mutex);
        shared->result = r; shared->done = true;
    }
};
}
