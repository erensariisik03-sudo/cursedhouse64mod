#include "message_gate.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace message_gate;
    Context c; c.general_filter=true; c.name_filter=true; c.all_mode=true;
    c.own_name="Oyun AI"; c.sender="Ali"; c.other_players={"Veli"};
    auto d=Evaluate("ahahahah",c); assert(d.action==Action::Drop&&d.reason==Reason::Noise);
    d=Evaluate("Veli, kapıya gel",c); assert(d.action==Action::Drop&&d.reason==Reason::AddressedToOther);
    d=Evaluate("Oyun AI merhaba",c); assert(d.action==Action::Instant&&!d.instant_reply.empty());
    d=Evaluate("zeka nasılsın",c); assert(d.action==Action::Instant);
    d=Evaluate("yapay, kapı nerede?",c); assert(d.action==Action::Ai&&d.mentions_bot);
    d=Evaluate("kapı nerede?",c); assert(d.action==Action::Drop&&d.reason==Reason::NameRequired);
    d=Evaluate("selam",c); assert(d.action==Action::Instant&&d.reason==Reason::SmallTalk); // local greetings do not require a name mention
    d=Evaluate("zeka selam",c); assert(d.action==Action::Instant); // addressed greeting remains local
    c.name_filter=false; d=Evaluate("kapı nerede?",c); assert(d.action==Action::Ai);
    c.general_filter=false; d=Evaluate("ahahahah",c); assert(d.action==Action::Ai);
    d=Evaluate("selam",c); assert(d.action==Action::Instant); // greetings stay local even with general filter off
    c.general_filter=true; c.all_mode=false; c.name_filter=true;
    d=Evaluate("kapı nerede?",c); assert(d.action==Action::Ai); // selected-player mode is already audience scoped
    c.all_mode=true; c.in_conversation=true; c.side_conversation=true; c.name_filter=true;
    d=Evaluate("kapı nerede?",c); assert(d.action==Action::Ai); // ongoing bot conversation does not demand repeated name
    c.in_conversation=false; c.name_filter=false; d=Evaluate("evet",c); assert(d.action==Action::Drop&&d.reason==Reason::SideConversation);
    std::cout << "Shared message gate: general/name toggles, direct name aliases, greetings, laughs, other-player addressing and conversation context: PASS\\n";
}
