#pragma once
#include "il2cpp_runtime.h"
#include <array>
#include <string>
#include <vector>

namespace cosmetic_catalog {
// Unity scripting thread only. No renderer callbacks, native hooks or guessed RVAs.
enum class Phase { Delay, Waiting, Scan, Apply, Ready, Rollback, Disabled };
struct Status {
    Phase phase = Phase::Delay;
    std::array<size_t,3> catalog{};
    std::array<size_t,3> added{};
};
class Controller {
public:
    explicit Controller(const il2cpp::Api& api): api_(api) {}
    ~Controller();
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;
    void Tick(double seconds);
    const Status& GetStatus() const { return status_; }
private:
    using Object = il2cpp::Object;
    using Method = il2cpp::Method;
    using Handle = il2cpp::Handle;
    struct Source { Handle list{}; const Method *count{}, *item{}; int length{}, at{}; };
    struct Target { Handle list{}; const Method *count{}, *contains{}, *add{}, *remove{}; int final_count{}; };
    const il2cpp::Api& api_;
    Status status_{};
    bool initialized_ = false, started_ = false, catalog_ready_ = false;
    bool notifying_ = false;
    double next_ = 0;
    int attempts_ = 0;
    il2cpp::Class *library_class_{}, *profile_class_{}, *string_class_{}, *cache_class_{}, *action_class_{};
    il2cpp::Field* changed_event_{};
    const Method* changed_invoke_{};
    std::array<const Method*,3> view_get_{};
    const Method *library_get_{}, *profile_get_{};
    std::array<const Method*,3> lists_get_{};
    std::array<Source,3> sources_{};
    std::array<Target,3> targets_{};
    std::array<std::vector<std::string>,3> ids_{};
    std::array<std::vector<std::string>,3> inserted_{};
    Handle profile_{};
    size_t category_ = 0, index_ = 0;
    bool Resolve();
    Object* Library();
    Object* Read(Object*, const char*);
    Object* Call0(const Method*, Object* = nullptr);
    Object* List(size_t, Object*);
    bool Integer(const Method*, Object*, int&);
    bool Boolean(const Method*, Object*, Object*, bool&);
    bool StartScan(Object*);
    bool Scan();
    bool Begin(Object*);
    bool SameProfile();
    bool Apply();
    void NotifyChanged();
    bool ReadWardrobeList(size_t category, std::vector<std::string>& out);
    bool RefreshWardrobeLists();
    void Rollback();
    void Free(Handle&);
    void ClearSources();
    void ClearTargets();
    void Wait(double, const char*);
    void Fail(const char*);
};
void Pump(const il2cpp::Api&);
}
