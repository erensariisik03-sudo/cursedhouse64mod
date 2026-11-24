#pragma once
// GMM photo receiver: independent, permission-free password gate.
// This protects the local *receiver mode*. It is not image encryption or a
// substitute for authenticated Photon event recipients.
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>
namespace photo_transfer_security {
using Bytes=std::vector<uint8_t>;
inline uint32_t R(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
class Sha256 {
    std::array<uint32_t,8> h_{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::array<uint8_t,64> buf_{};uint64_t length_=0;size_t used_=0;
    void Block(const uint8_t* b){
        static constexpr uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64];for(int i=0;i<16;++i)w[i]=(uint32_t(b[4*i])<<24)|(uint32_t(b[4*i+1])<<16)|(uint32_t(b[4*i+2])<<8)|b[4*i+3];
        for(int i=16;i<64;++i){const auto s0=R(w[i-15],7)^R(w[i-15],18)^(w[i-15]>>3);const auto s1=R(w[i-2],17)^R(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
        uint32_t a=h_[0],b0=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],h=h_[7];
        for(int i=0;i<64;++i){const auto s1=R(e,6)^R(e,11)^R(e,25);const auto ch=(e&f)^(~e&g);const auto t1=h+s1+ch+k[i]+w[i];const auto s0=R(a,2)^R(a,13)^R(a,22);const auto maj=(a&b0)^(a&c)^(b0&c);const auto t2=s0+maj;
            h=g;g=f;f=e;e=d+t1;d=c;c=b0;b0=a;a=t1+t2;}
        h_[0]+=a;h_[1]+=b0;h_[2]+=c;h_[3]+=d;h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=h;
    }
public:
    void Update(const uint8_t* bytes,size_t n){length_+=uint64_t(n);for(size_t i=0;i<n;++i){buf_[used_++]=bytes[i];if(used_==64){Block(buf_.data());used_=0;}}}
    void Update(const Bytes& b){Update(b.data(),b.size());}
    std::array<uint8_t,32> Finish(){const uint64_t bits=length_*8;buf_[used_++]=0x80;if(used_>56){while(used_<64)buf_[used_++]=0;Block(buf_.data());used_=0;}while(used_<56)buf_[used_++]=0;for(int i=7;i>=0;--i)buf_[used_++]=uint8_t(bits>>(i*8));Block(buf_.data());std::array<uint8_t,32> result{};for(int i=0;i<8;++i)for(int j=0;j<4;++j)result[4*i+j]=uint8_t(h_[i]>>(24-8*j));return result;}
};
inline std::array<uint8_t,32> Hash(const uint8_t* data,size_t n){Sha256 s;s.Update(data,n);return s.Finish();}
inline std::array<uint8_t,32> Hash(const Bytes& b){return Hash(b.data(),b.size());}
inline std::array<uint8_t,32> Hmac(const Bytes& key,const Bytes& msg){
    uint8_t block[64]{};if(key.size()>64){auto h=Hash(key);std::memcpy(block,h.data(),32);}else if(!key.empty())std::memcpy(block,key.data(),key.size());
    uint8_t outer[64],inner[64];for(int i=0;i<64;++i){outer[i]=block[i]^0x5c;inner[i]=block[i]^0x36;}
    Sha256 s;s.Update(inner,64);s.Update(msg);auto intermediate=s.Finish();Sha256 t;t.Update(outer,64);t.Update(intermediate.data(),32);return t.Finish();
}
inline std::array<uint8_t,32> Pbkdf2(const std::string& pass,const Bytes& salt,uint32_t iterations){
    Bytes key(pass.begin(),pass.end()),data=salt;data.insert(data.end(),{0,0,0,1});
    auto u=Hmac(key,data),v=u;for(uint32_t i=1;i<iterations;++i){Bytes tmp(u.begin(),u.end());u=Hmac(key,tmp);for(size_t j=0;j<v.size();++j)v[j]^=u[j];}return v;
}
inline std::string Hex(const uint8_t* p,size_t len){static const char* k="0123456789abcdef";std::string s;s.reserve(len*2);for(size_t i=0;i<len;++i){s.push_back(k[p[i]>>4]);s.push_back(k[p[i]&15]);}return s;}
inline int Digit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
inline bool Unhex(const std::string& s,Bytes& out){if(s.size()%2)return false;out.clear();out.reserve(s.size()/2);for(size_t i=0;i<s.size();i+=2){int a=Digit(s[i]),b=Digit(s[i+1]);if(a<0||b<0)return false;out.push_back(uint8_t(a*16+b));}return true;}
inline bool Same(const uint8_t* a,const uint8_t* b,size_t n){uint8_t diff=0;for(size_t i=0;i<n;++i)diff|=a[i]^b[i];return diff==0;}
class Gate {
    Bytes salt_;std::array<uint8_t,32> check_{};bool configured_=false,unlocked_=false,receive_=false;uint8_t failed_=0;
public:
    static constexpr uint32_t kIterations=100000;
    bool Configured()const{return configured_;}bool Unlocked()const{return unlocked_;}bool Receiving()const{return receive_&&unlocked_;}
    // This is an authorization gate. The transfer layer must ALSO check the
    // target actor, Photon server-attested sender, explicit offer and bytes.
    bool Create(const std::string& password){if(password.size()<8||password.size()>128)return false;
        std::random_device rng;salt_.resize(16);for(auto& b:salt_)b=uint8_t(rng());check_=Pbkdf2(password,salt_,kIterations);configured_=true;unlocked_=receive_=false;failed_=0;return true;}
    bool Load(const std::string& contents){const auto sep=contents.find(':');if(sep!=32||contents.size()!=97)return false;Bytes salt,hash;if(!Unhex(contents.substr(0,sep),salt)||!Unhex(contents.substr(sep+1),hash)||salt.size()!=16||hash.size()!=32)return false;
        salt_=std::move(salt);std::memcpy(check_.data(),hash.data(),32);configured_=true;unlocked_=receive_=false;failed_=0;return true;}
    std::string Save()const{return configured_?Hex(salt_.data(),salt_.size())+":"+Hex(check_.data(),32):"";}
    bool Unlock(const std::string& password){if(!configured_||failed_>=5||password.empty())return false;auto test=Pbkdf2(password,salt_,kIterations);
        if(Same(test.data(),check_.data(),32)){unlocked_=true;failed_=0;return true;}
        ++failed_;unlocked_=receive_=false;return false;}
    bool SetReceiving(bool enable){if(enable&&!unlocked_)return false;receive_=enable;return true;}
    void Lock(){unlocked_=receive_=false;}
    void Reset(){*this=Gate{};}
    int AttemptsRemaining()const{return 5-int(failed_);}
};
constexpr size_t kMaxImageBytes=256*1024;
constexpr size_t kChunkBytes=768;
constexpr size_t kMaxChunks=(kMaxImageBytes+kChunkBytes-1)/kChunkBytes;
inline bool IsImage(const Bytes& b){
    if(b.size()<8||b.size()>kMaxImageBytes)return false;
    const bool jpeg=b[0]==0xff&&b[1]==0xd8&&b[b.size()-2]==0xff&&b.back()==0xd9;
    const uint8_t png[]={137,80,78,71,13,10,26,10};bool png_ok=true;for(size_t i=0;i<8;++i)if(b[i]!=png[i])png_ok=false;
    return jpeg||png_ok;
}
struct Offer {uint64_t id=0;int sender=0,recipient=0;size_t total=0;size_t chunks=0;std::array<uint8_t,32> sha{};};
class Reassembler {
    Offer offer_{};std::vector<Bytes> parts_;std::vector<bool> arrived_;size_t have_=0,bytes_=0;bool active_=false;
public:
    bool BeginTrusted(const Offer& info,int local_actor,bool receiver_approved){Cancel();if(!receiver_approved||local_actor<=0||info.recipient!=local_actor||!info.id||info.sender<=0||info.recipient<=0||info.total<8||info.total>kMaxImageBytes||info.chunks!=(info.total+kChunkBytes-1)/kChunkBytes||info.chunks>kMaxChunks)return false;
        offer_=info;parts_.resize(info.chunks);arrived_.assign(info.chunks,false);active_=true;return true;}
    bool Begin(const Offer& info,int local_actor,const Gate& gate){return BeginTrusted(info,local_actor,gate.Receiving());}
    bool Push(uint64_t id,int attested_sender,size_t index,const Bytes& part){if(!active_||id!=offer_.id||attested_sender!=offer_.sender||index>=offer_.chunks||part.empty()||part.size()>kChunkBytes)return false;
        const size_t expected=index+1==offer_.chunks?offer_.total-index*kChunkBytes:kChunkBytes;
        if(part.size()!=expected||arrived_[index])return false;
        parts_[index]=part;arrived_[index]=true;++have_;bytes_+=part.size();return true;}
    bool Finish(Bytes& out){if(!active_||have_!=offer_.chunks||bytes_!=offer_.total)return false;out.clear();out.reserve(bytes_);for(const auto& p:parts_)out.insert(out.end(),p.begin(),p.end());
        const auto h=Hash(out);const bool valid=Same(h.data(),offer_.sha.data(),32)&&IsImage(out);if(!valid)out.clear();Cancel();return valid;}
    size_t Received()const{return have_;}bool Active()const{return active_;}
    void Cancel(){offer_={};parts_.clear();arrived_.clear();have_=bytes_=0;active_=false;}
};
} // namespace photo_transfer_security
