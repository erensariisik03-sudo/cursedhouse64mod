#pragma once
#include <deque>
#include "myinstants_core.h"
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
inline std::mutex mutex;inline Snapshot snapshot;inline Config config;inline Settings desired;inline unsigned revision=0;inline bool cancel=false;inline std::deque<Text> texts;inline bool selection_submit=false,selection_skip=false;inline std::vector<int> selection_indices;inline std::deque<std::string> speaks;inline std::string speech_text;inline std::atomic<int> tone{50}; // 50 = natural pitch; no robot effect
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
struct Soundboard {std::string query,status="Bir ses ara.",search_pending,play_pending;std::vector<myinstants::Sound> results;unsigned generation=0;bool searching=false,loading=false;};
inline Soundboard soundboard;
inline Soundboard ReadSounds(){std::lock_guard<std::mutex> l(mutex);return soundboard;}
inline void SearchSounds(std::string text){std::lock_guard<std::mutex> l(mutex);if(text.empty()||text.size()>240)return;soundboard.query=text;soundboard.search_pending=std::move(text);soundboard.results.clear();++soundboard.generation;soundboard.searching=true;soundboard.status="Myinstants aranıyor...";}
inline void PlaySound(const std::string& path){std::lock_guard<std::mutex> l(mutex);if(!desired.enabled||soundboard.loading||!myinstants::ValidPath(path)||!std::any_of(soundboard.results.begin(),soundboard.results.end(),[&](const auto& s){return s.path==path;}))return;soundboard.play_pending=path;soundboard.loading=true;soundboard.status="Ses indiriliyor...";}
inline std::string preferred_voice;inline std::vector<std::string> available_voices;inline std::string actual_voice;
inline std::string PreferredVoice(){std::lock_guard<std::mutex> l(mutex);return preferred_voice;}
inline void ChooseVoice(std::string name){std::lock_guard<std::mutex> l(mutex);preferred_voice=std::move(name);}
inline void PublishVoices(std::vector<std::string> names,std::string actual){std::lock_guard<std::mutex> l(mutex);available_voices=std::move(names);actual_voice=std::move(actual);}
inline std::pair<std::vector<std::string>,std::string> Voices(){std::lock_guard<std::mutex> l(mutex);return {available_voices,actual_voice};}
inline void SetTone(int v){tone.store(std::clamp(v,0,100));}
inline void SaveSpeech(std::string text){std::lock_guard<std::mutex> l(mutex);speech_text=std::move(text);} // saving never speaks
inline std::string SpeechText(){std::lock_guard<std::mutex> l(mutex);return speech_text;}
// Remember the typed message; speak it now if voice AI is on. Returns false when it could only be saved.
inline bool Speak(std::string text){std::lock_guard<std::mutex> l(mutex);speech_text=text;if(text.empty()||!desired.enabled)return false;if(speaks.size()<2)speaks.push_back(std::move(text));return true;}
inline void Stop(){std::lock_guard<std::mutex> l(mutex);cancel=true;++revision;}
inline bool RoutesText(){std::lock_guard<std::mutex> l(mutex);return desired.enabled&&desired.input!=0;}
inline void PushText(Text t){std::lock_guard<std::mutex> l(mutex);if(desired.enabled&&desired.input!=0&&texts.size()<4)texts.push_back(std::move(t));}
}
