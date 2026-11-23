#pragma once
// Deep, robotic voice for the AI's spoken replies.
// Pure DSP, runs on the worker thread (never touches Unity/IL2CPP).
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>
namespace voice_fx {
constexpr double kPi=3.14159265358979323846;
constexpr double kPitchRatio=.62; // ~ -8 semitones: a female TTS voice ends up as a deep male/giant voice
// Linear-interpolation resampler. step>1 = faster/higher, step<1 = slower/lower.
inline std::vector<float> Resample(const std::vector<float>& in,double step){
 if(in.size()<2||!(step>0))return in;
 const size_t n=size_t(double(in.size()-1)/step)+1;std::vector<float> out(n);
 for(size_t i=0;i<n;++i){const double pos=double(i)*step;const size_t j=size_t(pos);if(j+1>=in.size()){out[i]=in.back();continue;}out[i]=in[j]+(in[j+1]-in[j])*float(pos-double(j));}
 return out;
}
// WSOLA time stretch: output length ~= input length * alpha, pitch unchanged.
inline std::vector<float> Stretch(const std::vector<float>& in,double alpha,int rate){
 const size_t N=std::max<size_t>(128,size_t(rate*.040)&~size_t(1)),H=N/2,reach=N/3;
 if(in.size()<N*3||!(alpha>0.05)||alpha>20)return in;
 std::vector<float> win(N);for(size_t i=0;i<N;++i)win[i]=float(.5-.5*std::cos(2*kPi*double(i)/double(N)));
 std::vector<float> out(size_t(double(in.size())*alpha)+3*N,0.f);
 for(size_t i=0;i<N;++i)out[i]+=win[i]*in[i];
 const double hop=double(H)/alpha;size_t prev=0,end=N;
 for(size_t k=1;;++k){
  const double nominal=double(k)*hop;
  if(nominal+double(N+reach)>=double(in.size()))break;
  const size_t out_pos=k*H;if(out_pos+N>out.size())break;
  const size_t mid=size_t(nominal),lo=mid>reach?mid-reach:0,hi=std::min(mid+reach,in.size()-N);
  const float* ref=&in[prev+H]; // natural continuation of the previous grain
  size_t best=std::min(std::max(mid,lo),hi);double best_score=-1e300;
  for(size_t c=lo;c<=hi;c+=2){double dot=0,energy=1e-9;for(size_t i=0;i<H;i+=4){dot+=double(in[c+i])*ref[i];energy+=double(in[c+i])*in[c+i];}const double score=dot/std::sqrt(energy);if(score>best_score){best_score=score;best=c;}}
  for(size_t i=0;i<N;++i)out[out_pos+i]+=win[i]*in[best+i];
  prev=best;end=out_pos+N;
 }
 out.resize(end);return out;
}
// Lower the pitch (and vocal-tract size) without changing the duration.
inline std::vector<float> PitchDown(const std::vector<float>& in,int rate,double ratio=kPitchRatio){
 if(in.size()<2||rate<8000||!(ratio>0.2&&ratio<1))return in;
 std::vector<float> padded=in;padded.resize(in.size()+size_t(rate)*.1,0.f); // WSOLA drops the last partial grain
 auto out=Resample(Stretch(padded,ratio,rate),ratio);
 if(out.size()>in.size()+size_t(rate)/10)out.resize(in.size()+size_t(rate)/10);
 return out;
}
inline void Normalize(std::vector<float>& s,float peak){float m=0;for(float v:s)m=std::max(m,std::fabs(v));if(m>1e-6f){const float g=peak/m;for(float& v:s)v*=g;}}
// Metallic "robot" colouring: bit/sample crush, 55 Hz amplitude flutter, short comb, soft clip.
inline void Robot(std::vector<float>& s,int rate){
 if(s.empty()||rate<8000)return;
 double px=0,py=0;for(float& v:s){const double x=v;py=x-px+.995*py;px=x;v=float(py);} // DC / rumble block
 Normalize(s,.8f);
 const size_t hold=std::max<size_t>(1,size_t(std::lround(double(rate)/9000)));float held=0;
 for(size_t i=0;i<s.size();++i){if(i%hold==0)held=s[i];s[i]=std::round(held*64.f)/64.f;}
 const double w=2*kPi*55./double(rate);for(size_t i=0;i<s.size();++i)s[i]*=float(.55+.45*std::sin(w*double(i)));
 const size_t d=std::max<size_t>(1,size_t(double(rate)*.0045));for(size_t i=s.size();i-->d;)s[i]+=.5f*s[i-d];
 for(float& v:s)v=std::tanh(2.f*v);
 Normalize(s,.9f);
}
inline void DeepRobot(std::vector<float>& s,int rate){
 for(float& v:s)if(!std::isfinite(v))v=0;
 if(s.empty()||rate<8000||rate>48000)return;
 auto out=PitchDown(s,rate);Robot(out,rate);s=std::move(out);
}
}
