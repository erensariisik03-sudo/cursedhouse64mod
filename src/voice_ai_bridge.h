#pragma once
#include <deque>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
namespace voice_ai_bridge {
struct Settings { bool enabled=false;int input=2,output=1,destination=0; }; // input: audio/text/both; output: chat/voice; destination: auto/local
struct Snapshot {Settings settings;bool ready=false,network_ready=false,busy=false,speaking=false,mic_blocked=false;unsigned streams=0;std::string status="Sesli AI kapalı.",heard,reply;};
struct Config {std::string key,system,own_name;bool all=false,strict=true;std::vector<std::string> selected;};
struct Text {uintptr_t room=0;std::string name,text;};
inline std::mutex mutex;inline Snapshot snapshot;inline Config config;inline Settings desired;inline unsigned revision=0;inline bool cancel=false;inline std::deque<Text> texts;inline std::deque<std::string> speaks;inline std::string speech_text;inline std::atomic<int> tone{0}; // tone: 0 = deep robot (default) .. 100 = baby
inline Snapshot Read(){std::lock_guard<std::mutex> l(mutex);return snapshot;}
inline void Publish(Snapshot v){std::lock_guard<std::mutex> l(mutex);snapshot=std::move(v);}
inline void Configure(Config c){std::lock_guard<std::mutex> l(mutex);config=std::move(c);}
inline void Toggle(){std::lock_guard<std::mutex> l(mutex);desired.enabled=!desired.enabled;++revision;}
inline void Mode(int input,int output){std::lock_guard<std::mutex> l(mutex);desired.input=std::clamp(input,0,2);desired.output=std::clamp(output,0,1);++revision;}
inline void Destination(int v){std::lock_guard<std::mutex> l(mutex);desired.destination=std::clamp(v,0,1);++revision;}
inline void SetTone(int v){tone.store(std::clamp(v,0,100));}
inline std::string SpeechText(){std::lock_guard<std::mutex> l(mutex);return speech_text;}
// Remember the typed message; speak it now if voice AI is on. Returns false when it could only be saved.
inline bool Speak(std::string text){std::lock_guard<std::mutex> l(mutex);speech_text=text;if(text.empty()||!desired.enabled)return false;if(speaks.size()<2)speaks.push_back(std::move(text));return true;}
inline void Stop(){std::lock_guard<std::mutex> l(mutex);cancel=true;++revision;}
inline bool RoutesText(){std::lock_guard<std::mutex> l(mutex);return desired.enabled&&desired.input!=0;}
inline void PushText(Text t){std::lock_guard<std::mutex> l(mutex);if(desired.enabled&&desired.input!=0&&texts.size()<4)texts.push_back(std::move(t));}
}
