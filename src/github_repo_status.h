#pragma once
// Manual, read-only developer diagnostics. Never disables the game or mod.
namespace github_repo_status {
enum class State { NotChecked, Checking, Public, NotPublic, NetworkError };
struct Snapshot { State state = State::NotChecked; int http_code = 0; };
constexpr State Classify(bool connected, int http_code) {
    if (!connected) return State::NetworkError;
    if (http_code == 200) return State::Public;
    if (http_code == 404) return State::NotPublic; // May mean private, missing or no access.
    return State::NetworkError;
}
Snapshot Read();
bool CheckAsync(); // On-demand only. Never run automatically on a timer.
} // namespace github_repo_status
