#pragma once
// Android's own soft keyboard for panel text fields (API key, persona, own voice message).
// Uses UnityEngine.TouchScreenKeyboard through IL2CPP, so no TMP field and no ImGui-drawn keyboard is needed.
// Open() returns false when the game build lacks the API; callers then fall back to the ImGui keyboard.
// Unity-thread only (same thread as chat_bot::Pump).
#include "il2cpp_runtime.h"
#include <string>
namespace android_text_input {
enum class State { Open, Done, Canceled };
class Session {
    const il2cpp::Api& api;
    const il2cpp::Method *open_ = nullptr, *get_text_ = nullptr, *get_status_ = nullptr, *set_active_ = nullptr;
    il2cpp::Handle handle_ = 0;
    bool usable_ = false;
    void Resolve() { // cheap; redone on every Open() so a class that loads late is still found
        usable_ = false;
        open_ = get_text_ = get_status_ = set_active_ = nullptr;
        auto* klass = api.Find("UnityEngine", "TouchScreenKeyboard");
        if (!klass || !api.string_new || !api.gchandle_new || !api.gchandle_get_target || !api.gchandle_free || !api.object_unbox) return;
        for (int count : {8, 7, 6, 5, 4, 3, 2, 1}) if ((open_ = api.FindMethod(klass, "Open", count))) break; // (text,type,autocorrect,multiline,secure,alert,placeholder,limit)
        get_text_ = api.FindMethod(klass, "get_text", 0);
        get_status_ = api.FindMethod(klass, "get_status", 0); // Visible=0, Done=1, Canceled=2, LostFocus=3
        set_active_ = api.FindMethod(klass, "set_active", 1);
        usable_ = open_ && get_text_ && get_status_ && set_active_;
    }
    struct Rooted {
        const il2cpp::Api& api; il2cpp::Handle h;
        Rooted(const il2cpp::Api& a, il2cpp::Object* o) : api(a), h(o ? a.gchandle_new(o, false) : 0) {}
        ~Rooted() { if (h) api.gchandle_free(h); }
        il2cpp::Object* Get() const { return h ? api.gchandle_get_target(h) : nullptr; }
        Rooted(const Rooted&) = delete;
    };
public:
    explicit Session(const il2cpp::Api& a) : api(a) {}
    ~Session() { Close(); }
    Session(const Session&) = delete;
    bool Active() const { return handle_ != 0; }
    bool Open(const std::string& text, bool secret) {
        Close();
        Resolve();
        if (!usable_) return false;
        Rooted value(api, reinterpret_cast<il2cpp::Object*>(api.string_new(text.c_str())));
        Rooted placeholder(api, reinterpret_cast<il2cpp::Object*>(api.string_new("")));
        if (!value.Get() || !placeholder.Get()) return false;
        int type = 0, limit = 0;
        bool autocorrect = !secret, multiline = false, secure = secret, alert = false;
        void* args[] = {value.Get(), &type, &autocorrect, &multiline, &secure, &alert, placeholder.Get(), &limit};
        bool ok = false;
        il2cpp::Object* keyboard = api.Call(open_, nullptr, args, &ok);
        if (!ok || !keyboard) return false;
        handle_ = api.gchandle_new(keyboard, false);
        return handle_ != 0;
    }
    // Open: still typing. Done: Enter/Done pressed (text valid). Canceled: back / lost focus / keyboard gone.
    State Poll(std::string& text) {
        il2cpp::Object* keyboard = handle_ ? api.gchandle_get_target(handle_) : nullptr;
        if (!keyboard) return State::Canceled;
        bool ok = false;
        auto* current = api.Call(get_text_, keyboard, nullptr, &ok);
        if (ok) text = current ? api.Utf8(reinterpret_cast<il2cpp::String*>(current)) : std::string();
        auto* status = api.Call(get_status_, keyboard, nullptr, &ok);
        if (!ok || !status) return State::Canceled;
        const int value = *static_cast<int*>(api.object_unbox(status));
        return value == 0 ? State::Open : value == 1 ? State::Done : State::Canceled;
    }
    void Close() {
        if (!handle_) return;
        if (il2cpp::Object* keyboard = api.gchandle_get_target(handle_)) {
            bool off = false; void* args[] = {&off};
            api.Call(set_active_, keyboard, args, nullptr);
        }
        api.gchandle_free(handle_);
        handle_ = 0;
    }
};
}
