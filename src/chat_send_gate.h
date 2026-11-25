#pragma once
#include <atomic>
#include "il2cpp_runtime.h"
namespace chat_send_gate {
// True only after the local ChatManager.SendMessage hook was installed.
inline std::atomic<bool> ready{false};
// Only a synchronous decorative chat send may use this rooted display alias.
// Never write Photon player properties or the account nickname.
inline std::atomic<bool> status_alias_ready{false};
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
// Thread-local tag for chat sent by AI, vote notices, voice responses or status
// messages. Only a genuine player Send button/IME Done preserves its draft.
inline thread_local unsigned automated_send_depth=0;
struct AutomatedSend {
    AutomatedSend(){++automated_send_depth;}
    ~AutomatedSend(){--automated_send_depth;}
    AutomatedSend(const AutomatedSend&)=delete;
};
inline bool IsAutomated(){return automated_send_depth!=0;}
// Keep the game's normal cooldown for AI. The previous `ready || allowed`
// returned true for every AI tick once the hook was installed, causing spam.
inline bool Available(bool game_allowed) {
    return game_allowed;
}
}
