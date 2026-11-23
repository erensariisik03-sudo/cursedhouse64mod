#pragma once
#include "chat_bot_core.h"
#include <string>
#include <vector>

// One deterministic, allocation-light decision layer shared by chat and voice.
// The expensive speech recognizer is deliberately not part of this header.
namespace message_gate {
enum class Action { Drop, Instant, Warn, Ai };
enum class Reason { None, Empty, Noise, AddressedToOther, Profanity, SmallTalk, NameRequired, SideConversation };
struct Context {
    bool general_filter = true;
    bool name_filter = true;
    bool all_mode = false;
    bool in_conversation = false;
    bool side_conversation = false;
    int persona = 0;
    std::string sender;
    std::string own_name;
    std::vector<std::string> other_players;
};
struct Decision {
    Action action = Action::Ai;
    Reason reason = Reason::None;
    bool mentions_bot = false;
    std::string instant_reply;
};
inline Decision Evaluate(const std::string& raw, const Context& c) {
    Decision d;
    const std::string message = chat_bot_core::Trim(chat_bot_core::StripTags(raw));
    if (message.empty()) { d.action = Action::Drop; d.reason = Reason::Empty; return d; }
    d.mentions_bot = chat_bot_core::MentionsBot(message, c.own_name);

    // General filter can be switched off independently of name addressing.
    if (c.general_filter && chat_bot_core::IsNoise(message)) {
        d.action = Action::Drop; d.reason = Reason::Noise; return d;
    }
    if (c.general_filter && !d.mentions_bot &&
        chat_bot_core::AddressedToOther(message, c.other_players, c.own_name)) {
        d.action = Action::Drop; d.reason = Reason::AddressedToOther; return d;
    }
    if (c.general_filter && c.all_mode && !d.mentions_bot &&
        !c.in_conversation && c.side_conversation) {
        d.action = Action::Drop; d.reason = Reason::SideConversation; return d;
    }

    if (chat_bot_core::DirectedProfanity(message, c.own_name, c.other_players, c.in_conversation)) {
        d.action = Action::Warn; d.reason = Reason::Profanity; return d;
    }

    // Preserve the project's established behavior: complete greetings and
    // short pleasantries are answered locally, even when they do not mention
    // the bot name. The name gate controls only messages sent to the AI.
    if (auto reply = chat_bot_core::SmallTalkReply(message, c.sender, c.own_name, c.persona)) {
        d.action = Action::Instant; d.reason = Reason::SmallTalk; d.instant_reply = std::move(*reply); return d;
    }

    // Audience selection already scopes a private conversation. In room-wide
    // mode require a bot name only for normal AI requests, not local greetings.
    if (c.all_mode && c.name_filter && !d.mentions_bot && !c.in_conversation) {
        d.action = Action::Drop; d.reason = Reason::NameRequired; return d;
    }
    return d;
}
} // namespace message_gate
