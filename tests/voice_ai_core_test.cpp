#include "voice_ai_core.h"
#include <cassert>
#include <iostream>
using namespace voice_ai_core;
namespace {
// Speech-like 20 ms frames: harmonic voice with a syllable-rate envelope.
std::vector<float> VoiceFrame(int frame,int rate=16000){std::vector<float> f(rate/50);for(size_t i=0;i<f.size();++i){const double t=(double(frame)*f.size()+i)/rate,env=.04+.2*std::fabs(std::sin(2*3.14159265*3*t));f[i]=float(env*(std::sin(2*3.14159265*140*t)+.5*std::sin(2*3.14159265*280*t)+.3*std::sin(2*3.14159265*420*t))/1.6);}return f;}
double Pitch(const std::vector<float>& s,int rate,double lo=60,double hi=400){const size_t at=s.size()/2-4096,n=8192;double best=-1e300;int lag=0;for(int l=int(rate/hi);l<=int(rate/lo);++l){double d=0;for(size_t i=0;i<n;++i)d+=double(s[at+i])*s[at+i+l];if(d>best){best=d;lag=l;}}return double(rate)/lag;}
std::shared_ptr<std::atomic<bool>> cancel_after_http;int calls=0;bool fail_tts=false,unaddressed=false;std::vector<std::string> requests;
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
 Segmenter segmenter;std::vector<float> silent(4800,0);Utterance utterance;
 segmenter.Push(1,"Ali",silent.data(),silent.size(),48000,1,0);segmenter.Flush(1);assert(!segmenter.Take(utterance));
 std::vector<float> long_speech(19200,.1f);for(int i=0;i<5;++i)segmenter.Push(1,"Ali",long_speech.data(),long_speech.size(),48000,1,.4*i);
 segmenter.Flush(3);assert(segmenter.Take(utterance)&&utterance.actor==1&&utterance.samples.size()>=32000&&utterance.samples.size()<=32000+3200); // 2 s of sound + up to 0.2 s pre-rollassert(!segmenter.Take(utterance));
 segmenter.Clear();std::vector<float> stereo(19200,.2f);for(int i=0;i<4;++i)segmenter.Push(2,"Ayşe",stereo.data(),stereo.size(),16000,2,.6*i,i==3);assert(segmenter.Take(utterance));segmenter.Clear();assert(!segmenter.Take(utterance));

 // --- Noise gate: only plausible speech becomes a clip -----------------------------------------------------------
 {Segmenter g;Utterance u;unsigned seed=1;auto rnd=[&]{seed=seed*1664525u+1013904223u;return float((seed>>8)&0xFFFF)/65535.f*2-1;};
  for(int i=0;i<100;++i){std::vector<float> hiss(320);for(auto& v:hiss)v=.2f*rnd();g.Push(1,"A",hiss.data(),hiss.size(),16000,1,i*.02);}g.Flush(10);assert(!g.Take(u)); // hiss
  for(int i=0;i<100;++i){std::vector<float> click(320,0.f);if(i%10==0)click[7]=.9f;g.Push(1,"A",click.data(),click.size(),16000,1,i*.02);}g.Flush(10);assert(!g.Take(u)); // taps / clicks
  for(int i=0;i<75;++i){std::vector<float> tone(320);for(size_t k=0;k<tone.size();++k)tone[k]=.2f*float(std::sin(2*3.14159265*440*(i*320.+k)/16000));g.Push(1,"A",tone.data(),tone.size(),16000,1,i*.02);}g.Flush(10);assert(!g.Take(u)); // steady tone
  for(int i=0;i<60;++i){auto f=VoiceFrame(i);g.Push(1,"A",f.data(),f.size(),16000,1,i*.02);}g.Flush(10);assert(!g.Take(u)); // 1.2 s of sound is under the 2 s rule
  for(int i=0;i<120;++i){auto f=VoiceFrame(i);g.Push(1,"A",f.data(),f.size(),16000,1,i*.02);}g.Flush(10);assert(g.Take(u)&&u.actor==1&&u.samples.size()>=38400&&u.samples.size()<=38400+2400+320); // real speech, silence tail trimmed
 }
 // A 0.5 s pause inside one phrase does not cut it: 1.5 s + 0.5 s gap + 1.5 s = ONE clip; a 1.5 s pause splits (each half < 2 s -> both dropped)
 {Segmenter g;Utterance u;std::vector<float> quiet(320,0.f);int t=0;auto speak=[&](int n){for(int i=0;i<n;++i,++t){auto f=VoiceFrame(t);g.Push(6,"C",f.data(),f.size(),16000,1,t*.02);}};auto pause=[&](int n){for(int i=0;i<n;++i,++t)g.Push(6,"C",quiet.data(),quiet.size(),16000,1,t*.02);};
  speak(75);pause(25);speak(75);g.Flush(t*.02+1);assert(g.Take(u)&&u.samples.size()>=16000*3.4&&!g.Take(u));
  speak(75);pause(75);speak(75);g.Flush(t*.02+1);assert(!g.Take(u));}
 // --- 12 s cap: one clip of exactly 12 s, handed over without waiting for silence ----------------------------
 {Segmenter g;Utterance u;for(int i=0;i<650;++i){auto f=VoiceFrame(i);g.Push(3,"B",f.data(),f.size(),16000,1,i*.02);}assert(g.Take(u)&&u.samples.size()==kMaxSamples&&u.samples.size()==16000*12);}
 // --- Requests queued while the AI is busy merge into one clip (max 12 s) ---------------------------------
 {Request a,b;a.actor=b.actor=4;a.epoch=b.epoch=1;a.room=b.room=9;a.pcm.assign(16000*3,.1f);b.pcm.assign(16000*2,.1f);assert(Merge(a,b)&&a.pcm.size()==16000*5+4800);
  Request c=a;c.actor=5;assert(!Merge(a,c));Request big;big.actor=4;big.epoch=1;big.room=9;big.pcm.assign(16000*8,.1f);assert(!Merge(a,big));}
 // --- Deep robotic voice: pitch drops by kPitchRatio, duration and level stay sane ------------------------------
 {const int rate=24000;std::vector<float> v(rate*2);for(size_t i=0;i<v.size();++i){const double t=double(i)/rate;v[i]=float(.3*std::sin(2*3.14159265*220*t)+.15*std::sin(2*3.14159265*440*t)+.1*std::sin(2*3.14159265*660*t));}
  const auto low=voice_fx::PitchDown(v,rate);assert(low.size()>=v.size()*.95&&low.size()<=v.size()+rate/10+1);const double hz=Pitch(low,rate);assert(hz>220*.62*.92&&hz<220*.62*1.08);
  auto robot=v;voice_fx::DeepRobot(robot,rate);float peak=0;for(float f:robot){assert(std::isfinite(f));peak=std::max(peak,std::fabs(f));}assert(peak>.85f&&peak<=.91f&&robot.size()>=v.size()*.95&&robot.size()<=v.size()+rate/10+1);
  std::vector<float> tiny(50,.2f);voice_fx::DeepRobot(tiny,rate);assert(!tiny.empty());std::vector<float> none;voice_fx::DeepRobot(none,rate);assert(none.empty());}

 // --- Tone slider: 0 = current deep robot, 100 = baby; pitch follows the slider, robot colour fades out ----------
 {using namespace voice_fx;assert(std::fabs(ToneRatio(0)-kPitchRatio)<1e-9&&std::fabs(ToneRatio(1)-kBabyRatio)<1e-9&&ToneRatio(.5)>ToneRatio(.25)&&RobotAmount(0)==1&&RobotAmount(1)==0&&RobotAmount(.4)>0&&RobotAmount(.4)<1);
  const int rate=24000;std::vector<float> v(rate*2);for(size_t i=0;i<v.size();++i){const double t=double(i)/rate;v[i]=float(.3*std::sin(2*3.14159265*220*t)+.15*std::sin(2*3.14159265*440*t)+.1*std::sin(2*3.14159265*660*t));}
  auto baby=v;DeepRobot(baby,rate,1.0);float peak=0;for(float f:baby){assert(std::isfinite(f));peak=std::max(peak,std::fabs(f));}assert(peak>.85f&&peak<=.91f&&baby.size()>=v.size()*.95&&baby.size()<=v.size()+rate/10+1);
  const double hz=Pitch(baby,rate,220*kBabyRatio*.8,220*kBabyRatio*1.2);assert(hz>220*kBabyRatio*.92&&hz<220*kBabyRatio*1.08); // ~418 Hz: far higher than the 220 Hz input
  auto mid=v;DeepRobot(mid,rate,.5);const double hm=Pitch(mid,rate,220*.7,220*1.6);assert(hm>220*ToneRatio(.5)*.92&&hm<220*ToneRatio(.5)*1.08);
  auto same=v;DeepRobot(same,rate,0);assert(same.size()>=v.size()*.95);}
 // --- Own message: spoken directly (TTS + tone), zero HTTP / AI requests -------------------------------------------
 {Request m;m.epoch=3;m.room=5;m.speak=true;m.voice=true;m.name="Ben";m.text="Merhaba herkese!";m.tone=100;calls=0;auto r=Run(Http,m);assert(r.speak&&calls==0&&r.epoch==3&&r.room==5&&r.reply=="Merhaba herkese!"&&!r.audio.samples.empty()&&r.audio.rate==22050);
  m.tone=0;auto d=Run(Http,m);assert(d.audio.samples.size()>=1&&calls==0);
  m.text="   ";r=Run(Http,m);assert(r.skipped&&r.audio.samples.empty()&&calls==0);
  m.text="Merhaba";fail_tts=true;r=Run(Http,m);assert(r.speak&&r.audio.samples.empty()&&calls==0);fail_tts=false;}
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
 std::cout<<"Voice core: PCM/WAV/base64 validation, per-speaker bounded VAD/resampling/silence, audio-to-text-to-AI-to-speech, text/voice modes, TTS failure to text, key isolation, name gating, echo rejection, noise/click/tone/2 s-minimum gate, 12 s cap clip, queue merge, deep robot voice pitch/duration, tone slider deep→baby, own-message speech without AI call: PASS\n";
}
