#include "github_repo_status.h"
#include "chat_bot_http.h"
#include <atomic>
#include <thread>

namespace github_repo_status {
namespace {
constexpr const char* kEndpoint = "https://api.github.com/repos/erensariisik03-sudo/-ekup-reposu";
std::atomic<int> g_state{static_cast<int>(State::NotChecked)};
std::atomic<int> g_http_code{0};
std::atomic<bool> g_checking{false};
}
Snapshot Read() {
    return {static_cast<State>(g_state.load(std::memory_order_acquire)),
            g_http_code.load(std::memory_order_acquire)};
}
bool CheckAsync() {
    bool expected = false;
    if (!g_checking.compare_exchange_strong(expected, true)) return false;
    g_http_code.store(0, std::memory_order_release);
    g_state.store(static_cast<int>(State::Checking), std::memory_order_release);
    std::thread([] {
        int http_code = 0;
        const bool connected = chat_bot_http::GetStatus(kEndpoint, http_code);
        g_http_code.store(http_code, std::memory_order_release);
        g_state.store(static_cast<int>(Classify(connected, http_code)), std::memory_order_release);
        g_checking.store(false, std::memory_order_release);
    }).detach();
    return true;
}
} // namespace github_repo_status
