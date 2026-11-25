#pragma once
#include "chat_shortcut.h"
#include <string>
#include <vector>
namespace chat_bot_fx {
enum class Event { Start, Stop, Profile, Model };
inline constexpr double kInterval=1.0/6.0,kTimeout=20.0;
// Decorative terminal text only: never publish actual classes, APIs or provider IDs.
inline std::vector<std::string> Lines(Event event,const std::string& detail,bool enabled) {
    std::vector<std::string> lines={std::string("<#888F>")+chat_shortcut::kPayload};
    auto add=[&](const std::string& text){lines.push_back("<#EEEF>"+text);};
    switch(event){
    case Event::Start:
        add("root@aurora:~# install -d /run/aurora/staging /run/aurora/cache /run/aurora/session");
        add("root@aurora:~# cmake -S /opt/aurora-console -B /run/aurora/staging -G Ninja -DCMAKE_BUILD_TYPE=Release");
        add("-- Checking toolchain compatibility ... compiler accepted; release configuration selected");
        add("-- Resolving console dependencies ... local package manifest verified; dependency graph complete");
        add("-- Configuring done; generating done; build files written to /run/aurora/staging");
        add("root@aurora:~# cmake --build /run/aurora/staging --parallel 8 --target aurora-console");
        add("[ 1/12] Building CXX object console.dir/session/context.cpp.o      [release / optimized]");
        add("[ 2/12] Building CXX object console.dir/memory/arena.cpp.o         [allocation map ready]");
        add("[ 3/12] Building CXX object console.dir/text/normalizer.cpp.o      [unicode tables loaded]");
        add("[ 4/12] Building CXX object console.dir/dialogue/turn_queue.cpp.o  [ordered dispatch ready]");
        add("[ 5/12] Building CXX object console.dir/language/lexicon.cpp.o     [symbol index complete]");
        add("[ 6/12] Building CXX object console.dir/persona/style_map.cpp.o    [expression rules linked]");
        add("[ 7/12] Building CXX object console.dir/worker/scheduler.cpp.o    [worker pool configured]");
        add("[ 8/12] Building CXX object console.dir/terminal/renderer.cpp.o   [console output ready]");
        add("[ 9/12] Linking CXX static library libaurora-session.a            [object archive complete]");
        add("[10/12] Linking CXX executable aurora-console                     [symbol resolution complete]");
        add("[11/12] Validating release bundle and resource index              [all required files present]");
        add("[12/12] Installing console bundle into /run/aurora/session        [staged release committed]");
        add("root@aurora:~# auroractl session open --workspace /run/aurora/session --profile default");
        add("[  OK  ] Initialized session context; text pipeline ready; expression profile synchronized");
        add("root@aurora:~# auroractl start dialogue.service && auroractl wait --ready --timeout 30");
        add("[  OK  ] dialogue.service entered ready state; console input accepted; startup sequence complete");
        add(u8"[ SİSTEM ÇIKTISI ] AI AKTİF");break;
    case Event::Stop:
        add("root@aurora:~# auroractl session close --graceful --drain-queue --preserve-profile");
        add("[ INFO ] Shutdown transaction opened; console admission closed; pending work marked for drain");
        add("[  OK  ] Stopped accepting new dialogue jobs; existing session context remains available");
        add("[  OK  ] Dialogue queue drained successfully; no unfinished console jobs remain");
        add("root@aurora:~# auroractl stop dialogue.service --wait --timeout 30");
        add("[  OK  ] Language worker pool stopped; dispatch handles released; worker count returned to zero");
        add("[  OK  ] dialogue.service entered inactive state; automatic restart disabled for this session");
        add("root@aurora:~# auroractl journal flush --session current --checkpoint final");
        add("[  OK  ] Session journal synchronized; final checkpoint complete; write queue is empty");
        add("root@aurora:~# auroractl workspace release /run/aurora/session --temporary-only");
        add("[  OK  ] Temporary workspace released; persistent expression profile retained");
        add("[  OK  ] Session allocation arena cleared; cached dispatch tables detached");
        add("[  OK  ] Console channels closed cleanly; no active worker endpoints remain");
        add("root@aurora:~# auroractl session finalize --state offline --reason user-request");
        add("[  OK  ] Shutdown transaction committed; session state changed from stopping to offline");
        add("[ INFO ] Console shutdown complete; waiting for the next manual activation");
        add(u8"[ SİSTEM ÇIKTISI ] AI KAPANDI");break;
    case Event::Profile:{
        add("root@aurora:~# auroractl profile prepare --transaction --checkpoint /run/persona.previous");
        add("[  OK  ] Profile checkpoint created; current expression rules preserved until commit");
        add("[ INFO ] Preparing alternate expression package; active console session retained");
        add("root@aurora:~# aurora-pack verify /var/cache/persona.pkg --manifest --checksums");
        add("[  OK  ] Package structure verified; required configuration entries located");
        add("[  OK  ] Manifest checksums matched; profile archive accepted for staging");
        add("root@aurora:~# aurora-pack unpack /var/cache/persona.pkg --target /run/persona.next");
        add("  EXTRACT persona/voice.conf       [tone map loaded; response style entries resolved]");
        add("  EXTRACT persona/style.conf       [expression settings loaded; defaults merged]");
        add("  INDEX   persona/expression.map   [phrase index rebuilt; duplicate entries removed]");
        add("[  OK  ] Staged expression index complete; configuration consistency check passed");
        add("root@aurora:~# auroractl profile validate --staged /run/persona.next --strict");
        add("[  OK  ] Profile compatibility verified; staged configuration ready for activation");
        add("root@aurora:~# auroractl profile commit --atomic --source /run/persona.next");
        add("[  OK  ] Active expression profile switched; console session resumed with updated style");
        add("[  OK  ] Profile transaction committed successfully; temporary staging handles released");
        const std::string label=detail==u8"İmam"?u8"İMAM":detail==u8"Anı"?"ANI":detail=="Normal"?"NORMAL":detail==u8"Özel"?u8"ÖZEL":u8"YENİ";
        add(u8"[ SİSTEM ÇIKTISI ] MODEL: "+label+(enabled?u8" | AI AKTİF":u8" | AI KAPALI"));break;
    }
    case Event::Model:
        add("root@aurora:~# auroractl engine stage --alternate --checkpoint /run/engine.previous");
        add("[ INFO ] Preparing alternate engine slot; current console context remains attached");
        add("[  OK  ] Session checkpoint captured; ordered dialogue queue preserved for handover");
        add("root@aurora:~# aurora-loader inspect /opt/aurora/engine.next --manifest --segments");
        add("[  OK  ] Engine image manifest verified; segment layout and dispatch version accepted");
        add("  MAP     engine/text.segment     [read-execute view created; instruction index loaded]");
        add("  MAP     engine/data.segment     [session tables prepared; workspace references resolved]");
        add("  RELOC   engine/dispatch.table   [entry bindings resolved; relocation pass complete]");
        add("[  OK  ] Staged engine image mapped; compatibility checks completed without conflicts");
        add("root@aurora:~# auroractl engine warmup --staged --workers auto --check-ready");
        add("[  OK  ] Worker context initialized; dispatch loop ready; staged engine awaiting commit");
        add("[  OK  ] Readiness probe passed; console handover may proceed with the current session");
        add("root@aurora:~# auroractl engine switch --atomic --preserve-session --resume-queue");
        add("[  OK  ] Alternate engine slot activated; retained console context attached successfully");
        add("[  OK  ] Previous engine slot retired; temporary mapping references released");
        add("[  OK  ] Engine handover complete; ordered dialogue queue resumed; console ready");
        add(u8"[ SİSTEM ÇIKTISI ] AI MODELİ GÜNCELLENDİ");break;
    }
    return lines;
}
}
