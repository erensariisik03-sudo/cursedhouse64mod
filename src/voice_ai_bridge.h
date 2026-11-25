#pragma once
#include <deque>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <utility>
namespace voice_ai_bridge {
struct Settings { bool enabled=false, manual_keywords=false;int input=0,output=1,destination=0,target_actor=0; }; // input: audio/text/both; output: chat/voice; destination: auto/local
struct Snapshot {Settings settings;bool ready=false,network_ready=false,busy=false,speaking=false,mic_blocked=false;unsigned streams=0;int active_actor=0;std::vector<std::pair<int,std::string>> players;std::string status="Sesli AI kapalı.",heard,reply,selection_text,selection_name;};
struct Config {std::string key,system,own_name;bool all=false,strict=true;std::vector<std::string> selected;bool message_filter=true;};
struct Text {uintptr_t room=0;std::string name,text;bool instant=false;std::string instant_reply;};
inline std::mutex mutex;inline Snapshot snapshot;inline Config config;inline Settings desired;inline unsigned revision=0;inline bool cancel=false;inline std::deque<Text> texts;inline bool selection_submit=false,selection_skip=false;inline std::vector<int> selection_indices;inline std::deque<std::string> speaks;inline std::string speech_text;inline std::atomic<int> tone{0}; // tone: 0 = deep robot (default) .. 100 = baby
inline Snapshot Read(){std::lock_guard<std::mutex> l(mutex);return snapshot;}
inline void Publish(Snapshot v){std::lock_guard<std::mutex> l(mutex);snapshot=std::move(v);}
inline void Configure(Config c){std::lock_guard<std::mutex> l(mutex);config=std::move(c);}
inline void Toggle(){std::lock_guard<std::mutex> l(mutex);desired.enabled=!desired.enabled;++revision;}
inline void Enable(bool enabled){std::lock_guard<std::mutex> l(mutex);if(desired.enabled!=enabled){desired.enabled=enabled;++revision;}}
inline void ManualKeywords(bool enabled){std::lock_guard<std::mutex> l(mutex);desired.manual_keywords=enabled;++revision;}
inline void TargetActor(int actor){std::lock_guard<std::mutex> l(mutex);desired.target_actor=std::max(0,actor);++revision;}
inline void SubmitSelected(std::vector<int> words){std::lock_guard<std::mutex> l(mutex);selection_indices=std::move(words);selection_submit=true;selection_skip=false;}
inline void SkipSelected(){std::lock_guard<std::mutex> l(mutex);selection_skip=true;selection_submit=false;}
inline void Mode(int input,int output){std::lock_guard<std::mutex> l(mutex);desired.input=std::clamp(input,0,2);desired.output=std::clamp(output,0,1);++revision;}
inline void Destination(int v){std::lock_guard<std::mutex> l(mutex);desired.destination=std::clamp(v,0,1);++revision;}
inline void SetTone(int v){tone.store(std::clamp(v,0,100));}
inline void SaveSpeech(std::string text){std::lock_guard<std::mutex> l(mutex);speech_text=std::move(text);} // saving never speaks
inline std::string SpeechText(){std::lock_guard<std::mutex> l(mutex);return speech_text;}
// Remember the typed message; speak it now if voice AI is on. Returns false when it could only be saved.
inline bool Speak(std::string text){std::lock_guard<std::mutex> l(mutex);speech_text=text;if(text.empty()||!desired.enabled)return false;if(speaks.size()<2)speaks.push_back(std::move(text));return true;}
inline void Stop(){std::lock_guard<std::mutex> l(mutex);cancel=true;++revision;}
inline bool RoutesText(){std::lock_guard<std::mutex> l(mutex);return desired.enabled&&desired.input!=0;}
inline void PushText(Text t){std::lock_guard<std::mutex> l(mutex);if(desired.enabled&&desired.input!=0&&texts.size()<4)texts.push_back(std::move(t));}
}
