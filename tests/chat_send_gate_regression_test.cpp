#include "chat_send_gate.h"
#include <cassert>
int main(){
 chat_send_gate::ready.store(true);
 // AI MUST obey the normal game's cooldown, even when hook exists.
 assert(!chat_send_gate::Available(false));
 assert(chat_send_gate::Available(true));
 assert(!chat_send_gate::IsAutomated());
 {chat_send_gate::AutomatedSend outer;assert(chat_send_gate::IsAutomated());
  {chat_send_gate::AutomatedSend inner;assert(chat_send_gate::IsAutomated());}
  assert(chat_send_gate::IsAutomated());}
 assert(!chat_send_gate::IsAutomated());
}
