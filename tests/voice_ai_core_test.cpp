#include "voice_ai_core.h"
#include <cassert>
#include <iostream>
using namespace voice_ai_core;
namespace {std::shared_ptr<std::atomic<bool>> cancel_after_http;int calls=0;bool fail_tts=false,unaddressed=false;std::vector<std::string> requests;
bool Http(const std::string& url,const std::string& key,const std::string& body,int& code,std::string& response){
 assert(key=="secret"&&url.find("secret")==std::string::npos&&body.find("secret")==std::string::npos);++calls;requests.push_back(body);code=200;
 if(body.find("HEARD:")!=std::string::npos&&body.find("inlineData")!=std::string::npos){assert(body.find("audio/wav")!=std::string::npos);response=unaddressed?R"({"candidates":[{"content":{"parts":[{"text":"HEARD: başka birine soruyorum\nREPLY: SKIP"}]}}]})":R"({"candidates":[{"content":{"parts":[{"text":"HEARD: Bot kapı nerede?\nREPLY: Sağdaki kapıya gidebilirsin."}]}}]})";}
 else if(body.find("Transcribe only")!=std::string::npos){assert(body.find("audio/wav")!=std::string::npos);response=R"({"candidates":[{"content":{"parts":[{"text":"Bot kapı nerede?"}]}}]})";}
 else response=R"({"candidates":[{"content":{"parts":[{"text":"Sağdaki kapıya gidebilirsin."}]}}]})";
 if(cancel_after_http)cancel_after_http->store(true);
 return true;
}}
bool LocalTts(const std::string& text,std::string& wav){assert(!text.empty());if(fail_tts)return false;wav=Wav(std::vector<float>(2400,.1f),22050);return true;}
int main(){
 tts=&LocalTts;
 for(size_t n=0;n<17;++n){std::string raw;for(size_t i=0;i<n;++i)raw+=char(i*37);std::string decoded;if(n)assert(Unbase64(Base64(raw),decoded)&&decoded==raw);}
 for(auto bad:{"a","!!!!","A=AA","AA=A","AAAA===="}){std::string decoded;assert(!Unbase64(bad,decoded));}
 Audio a;assert(DecodeAudio(Wav({0,.5f,-.5f,1,-1},16000),"audio/wav",a)&&a.rate==16000&&a.samples.size()==5&&std::fabs(a.samples[1]-.5)<.001);
 assert(!DecodeAudio("RIFFbad","audio/wav",a));assert(!DecodeAudio(std::string(7,'a'),"audio/l16",a));assert(!DecodeAudio("aa","audio/mp3",a));
 Segmenter segmenter;std::vector<float> silent(4800,0),speech(4800,.1f);Utterance utterance;
 segmenter.Push(1,"Ali",silent.data(),silent.size(),48000,1,0);segmenter.Flush(1);assert(!segmenter.Take(utterance));
 for(int i=0;i<4;++i)segmenter.Push(1,"Ali",speech.data(),speech.size(),48000,1,.1*i);
 segmenter.Flush(2);assert(segmenter.Take(utterance)&&utterance.actor==1&&utterance.samples.size()>=6400&&utterance.samples.size()<=9600);assert(!segmenter.Take(utterance));
 segmenter.Clear();std::vector<float> stereo(3200,.2f);for(int i=0;i<3;++i)segmenter.Push(2,"Ayşe",stereo.data(),stereo.size(),16000,2,.1*i,i==2);assert(segmenter.Take(utterance));segmenter.Clear();assert(!segmenter.Take(utterance));
 Request q;q.epoch=7;q.room=12;q.name="Ali";q.key="secret";q.system="Türkçe cevapla.";q.own_name="Bot";q.voice=true;q.pcm=std::vector<float>(6400,.15f);
 auto r=Run(Http,q);assert(r.epoch==7&&r.room==12&&r.heard=="Bot kapı nerede?"&&!r.reply.empty()&&!r.audio.samples.empty()&&r.audio.rate==22050&&calls==1);
 {auto u=q;u.strict=true;unaddressed=true;calls=0;auto ru=Run(Http,u);assert(ru.skipped&&ru.reply.empty()&&calls==1);unaddressed=false;}
 q.pcm.clear();q.text="Bot kapı nerede?";q.voice=false;calls=0;r=Run(Http,q);assert(calls==1&&r.audio.samples.empty()&&!r.reply.empty());
 q.voice=true;fail_tts=true;calls=0;r=Run(Http,q);assert(calls==1&&r.audio.samples.empty()&&!r.reply.empty());fail_tts=false;
 q.text="başka birine soruyorum";q.strict=true;calls=0;r=Run(Http,q);assert(r.skipped&&calls==0);
 q.strict=false;q.text="Sağdaki kapıya gidebilirsin!";q.history={{true,"Sağdaki kapıya gidebilirsin."}};r=Run(Http,q);assert(r.skipped&&calls==0); // reflected AI speech cannot loop
 q.history.clear();q.text="Bot kapı nerede?";q.canceled=std::make_shared<std::atomic<bool>>(true);calls=0;r=Run(Http,q);assert(r.skipped&&calls==0);
 q.canceled->store(false);cancel_after_http=q.canceled;r=Run(Http,q);assert(r.skipped&&r.reply.empty()&&calls==1);cancel_after_http.reset();q.canceled.reset();
 voice_ai_bridge::desired={};voice_ai_bridge::Mode(99,-1);assert(voice_ai_bridge::desired.input==2&&voice_ai_bridge::desired.output==0);voice_ai_bridge::desired={};
 std::cout<<"Voice core: PCM/WAV/base64 validation, per-speaker bounded VAD/resampling/silence, audio-to-text-to-AI-to-speech, text/voice modes, TTS failure to text, key isolation, name gating, echo rejection: PASS\n";
}
