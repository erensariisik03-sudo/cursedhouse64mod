#pragma once
#include "text_buffer.h"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

// Copies only. Unity owns the field/IME and applies commands on its scripting
// thread; the renderer never keeps managed objects or sends an old text snapshot.
namespace native_shortcuts {
enum class Kind { Insert, ReplaceAll };
struct Command { Kind kind; std::string text; bool quick_send=false; };
struct State {
    uint64_t session=0;
    bool active=false, limited=false;
    std::string text;
    int cursor=0, anchor=0; // UTF-8 byte boundaries, not rendered TMP caret indices.
    float bottom=0; // Available screen fraction above IME; 0 means use top fallback.
};
inline std::mutex mutex;
inline State state;
inline uint64_t serial=0;
inline std::deque<Command> commands;
inline uint64_t Begin() {
    std::lock_guard<std::mutex> lock(mutex);
    state={}; state.session=++serial; state.active=true; commands.clear();
    return state.session;
}
inline void End(uint64_t session) {
    std::lock_guard<std::mutex> lock(mutex);
    if(state.session!=session)return;
    state={};commands.clear();
}
inline State Read() { std::lock_guard<std::mutex> lock(mutex);return state; }
inline void Publish(uint64_t session, const std::string& text, int cursor, int anchor, float bottom, bool limited=false) {
    std::lock_guard<std::mutex> lock(mutex);
    if(!state.active||state.session!=session)return;
    state.text=text_buffer::Clean(text.c_str());
    state.cursor=text_buffer::Boundary(state.text.c_str(),cursor);
    state.anchor=text_buffer::Boundary(state.text.c_str(),anchor);
    state.bottom=std::clamp(bottom,0.f,1.f);state.limited=limited;
}
inline bool Queue(uint64_t session, Command command) {
    std::lock_guard<std::mutex> lock(mutex);
    if(!state.active||state.session!=session||commands.size()>=64)return false;
    command.text=text_buffer::Clean(command.text.c_str());
    commands.push_back(std::move(command));return true;
}
inline std::vector<Command> Take(uint64_t session) {
    std::lock_guard<std::mutex> lock(mutex);
    if(!state.active||state.session!=session)return {};
    std::vector<Command> result(commands.begin(),commands.end());commands.clear();return result;
}
// UTF-16 offsets are used by TMP/Android; UTF-8 offsets are used by the shortcut
// helpers. Never put a cursor in the middle of a surrogate pair or UTF-8 scalar.
inline int ToBytes(const std::string& text, int units) {
    size_t byte=0;int count=0;
    while(byte<text.size()) {
        const size_t n=text_buffer::Sequence(reinterpret_cast<const unsigned char*>(text.data()+byte),text.size()-byte);
        if(!n)break;
        const int width=n==4?2:1;
        if(count+width>units)break;
        byte+=n;count+=width;
    }
    return static_cast<int>(byte);
}
inline int ToUnits(const std::string& text, int bytes) {
    const int end=text_buffer::Boundary(text.c_str(),bytes);int units=0;
    for(int i=0;i<end;){
        const size_t n=text_buffer::Sequence(reinterpret_cast<const unsigned char*>(text.data()+i),text.size()-i);
        if(!n)break;
        units+=n==4?2:1;i+=static_cast<int>(n);
    }
    return units;
}
}
