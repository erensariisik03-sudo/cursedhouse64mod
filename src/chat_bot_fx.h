#pragma once
#include "chat_shortcut.h"
#include <string>
#include <vector>
namespace chat_bot_fx {
enum class Event { Start, Stop, Profile, Model };
inline constexpr double kInterval=1.0/6.0,kTimeout=5.0;
// Decorative terminal text only: never publish actual classes, APIs or provider IDs.
inline std::vector<std::string> Lines(Event event,const std::string& detail,bool enabled) {
    std::vector<std::string> lines={std::string("<#888F>")+chat_shortcut::kPayload};
    auto add=[&](const std::string& text){lines.push_back("<#EEEF>"+text);};
    switch(event){
    case Event::Start:
        add("[  OK  ] Loading /opt/aurora/kernel.img ...");
        add("root@aurora:~# mount -t neurofs /dev/core0 /mnt/logic");
        add("[  OK  ] Checking lattice blocks [||||||||||] 100%");
        add("root@aurora:~# aurora-init --quiet --session fresh");
        add("[  OK  ] Starting cognition.service ... done");
        add(u8"[ SİSTEM ÇIKTISI ] AI AKTİF");break;
    case Event::Stop:
        add("[  OK  ] Stopping cognition.service ...");
        add("root@aurora:~# sync && umount /mnt/logic");
        add("[  OK  ] Closing volatile journal ... done");
        add("root@aurora:~# poweroff --session-only");
        add(u8"[ SİSTEM ÇIKTISI ] AI KAPANDI");break;
    case Event::Profile:{
        add("root@aurora:~# aurora-pack --unseal /var/cache/persona.pkg");
        add("[  OK  ] Rebuilding expression lattice [||||||||||] 100%");
        add("root@aurora:~# auroractl reload persona.service --atomic");
        const std::string label=detail==u8"İmam"?u8"İMAM":detail==u8"Anı"?"ANI":detail=="Normal"?"NORMAL":detail==u8"Özel"?u8"ÖZEL":u8"YENİ";
        add(u8"[ SİSTEM ÇIKTISI ] MODEL: "+label+(enabled?u8" | AI AKTİF":u8" | AI KAPALI"));break;
    }
    case Event::Model:
        add("[  OK  ] Opening alternate cognition channel ...");
        add("root@aurora:~# auroractl switch --automatic --preserve-session");
        add("[  OK  ] Aligning neural clock [||||||||||] 100%");
        add(u8"[ SİSTEM ÇIKTISI ] AI MODELİ GÜNCELLENDİ");break;
    }
    return lines;
}
}
