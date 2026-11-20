#pragma once
#include "chat_bot_core.h"
#include "chat_shortcut.h"
#include <string>
#include <vector>
namespace chat_bot_fx {
enum class Event { Start, Stop, Profile, Model };
inline constexpr double kInterval=1.0/6.0,kTimeout=5.0;
inline std::vector<std::string> Lines(Event event,const std::string& detail,bool enabled) {
    const auto label=chat_bot_core::CleanReply(detail,72);
    std::vector<std::string> lines={std::string("<#888F>")+chat_shortcut::kPayload};
    auto add=[&](const std::string& text){lines.push_back("<#EEEF>"+text);};
    switch(event){
    case Event::Start:
        add("[AI::BOOT] session.open() -> Unity scripting thread / bridge: READY");
        add("[AI::LINK] IL2CPP metadata -> ChatManager + TMP_InputField: RESOLVED");
        add("[AI::INPUT] AndroidIME.bind(); target_limit = 1024; shortcuts = ONLINE");
        add("[AI::ROUTE] llm.configure(\""+label+"\"); request_state = WAITING");
        add("[AI::POLICY] local_replies.load(); name_filter.sync(); memory.prepare()");
        add("[AI::READY] artificial_intelligence = ENABLED; awaiting player messages...");break;
    case Event::Stop:
        add("[AI::STOP] session.accept_new_messages(false); response_loop = STOPPING");
        add("[AI::QUEUE] pending.clear(); reply_buffer.clear(); commands = DRAINED");
        add("[AI::GUARD] previous_request.results -> IGNORE; input_drafts -> PRESERVE");
        add("[AI::CHAT] automatic_replies = DISABLED; native_keyboard = AVAILABLE");
        add("[AI::IDLE] artificial_intelligence = OFF; control returned to player.");break;
    case Event::Profile:
        add("[AI::PROFILE] persona.swap(\""+label+"\"); profile_revision++");
        add("[AI::MEMORY] conversation.clear(); previous_responses.invalidate()");
        add("[AI::POLICY] personality.apply(); local_rules = READY");
        add(std::string("[AI::READY] profile = LOADED; state = ")+(enabled?"ACTIVE":"STANDBY"));break;
    case Event::Model:
        add("[AI::ROUTE] provider.response = OK; selected_model = \""+label+"\"");
        add("[AI::MODEL] runtime.model.sync(); fallback_route = UPDATED");
        add("[AI::MEMORY] conversation = RETAINED; output_policy = READY");
        add("[AI::READY] response_channel = ONLINE; model switch complete.");break;
    }
    return lines;
}
}
