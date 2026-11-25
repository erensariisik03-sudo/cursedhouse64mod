#pragma once
#include <atomic>
#include "il2cpp_runtime.h"
namespace chat_send_gate {
// True only after the local ChatManager.SendMessage hook was installed.
inline std::atomic<bool> ready{false};
// Only a synchronous decorative chat send may use this rooted display alias.
// Never write Photon player properties or the account nickname.
inline std::atomic<bool> status_alias_ready{false};
inline thread_local bool automatic_send=false;
struct AutomaticSend {
    bool previous;
    AutomaticSend():previous(automatic_send){automatic_send=true;}
    ~AutomaticSend(){automatic_send=previous;}
    AutomaticSend(const AutomaticSend&)=delete;
};
inline thread_local il2cpp::Handle status_alias=0;
inline thread_local il2cpp::Handle active_status_alias=0;
struct StatusAlias {
    il2cpp::Handle previous;
    explicit StatusAlias(il2cpp::Handle name):previous(status_alias){status_alias=name;}
    ~StatusAlias(){status_alias=previous;}
    StatusAlias(const StatusAlias&)=delete;
};
// Consume the request at SendMessage entry. A nested ordinary send gets no alias.
struct StatusSend {
    il2cpp::Handle previous;
    StatusSend():previous(active_status_alias){active_status_alias=status_alias;status_alias=0;}
    ~StatusSend(){active_status_alias=previous;}
    StatusSend(const StatusSend&)=delete;
};
inline bool Available(bool game_allowed) {
    return game_allowed||ready.load(std::memory_order_acquire);
}
}
