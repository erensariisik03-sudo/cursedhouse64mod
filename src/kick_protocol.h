#pragma once
#include "photo_transfer_security.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <charconv>
namespace kick_protocol {
// Legacy shared-key protocol retained only for backwards-compatibility tests.
// New room-host command is GMMKCK2 and requires independent MasterClient check.
namespace security=photo_transfer_security;
constexpr std::string_view kMagic="GMMKCK1";
inline std::string SignedBody(uint64_t nonce,int sender,int recipient){
    return std::string(kMagic)+"|K|"+std::to_string(nonce)+"|"+std::to_string(sender)+"|"+std::to_string(recipient);
}
inline std::string Encode(uint64_t nonce,int sender,int recipient,const security::Bytes& key){
    if(!nonce||sender<=0||recipient<=0||key.size()!=32)return {};
    const auto body=SignedBody(nonce,sender,recipient);
    const auto tag=security::Hmac(key,security::Bytes(body.begin(),body.end()));
    return body+"|"+security::Hex(tag.data(),tag.size());
}
inline bool Decode(const std::string& input,int attested_sender,int local_actor,const security::Bytes& key,uint64_t& nonce){
    nonce=0;
    if(key.size()!=32||input.size()>180||input.compare(0,8,"GMMKCK1|")!=0)return false;
    const auto tagpos=input.rfind('|');
    if(tagpos==std::string::npos||input.size()-tagpos-1!=64)return false;
    const std::string body=input.substr(0,tagpos);
    if(body.rfind(std::string(kMagic)+"|K|",0)!=0)return false;
    const auto npos=body.find('|',10),apos=body.find('|',npos==std::string::npos?0:npos+1);
    if(npos==std::string::npos||apos==std::string::npos)return false;
    uint64_t candidate=0;int sender=0,recipient=0;
    const auto parse=[](std::string_view v,auto& value)->bool{
        if(v.empty()||v.size()>20)return false;
        const auto res=std::from_chars(v.data(),v.data()+v.size(),value);
        return res.ec==std::errc{}&&res.ptr==v.data()+v.size();
    };
    if(!parse(std::string_view(body).substr(10,npos-10),candidate)||!candidate||
       !parse(std::string_view(body).substr(npos+1,apos-npos-1),sender)||
       !parse(std::string_view(body).substr(apos+1),recipient)||
       sender!=attested_sender||recipient!=local_actor)return false;
    security::Bytes supplied;
    if(!security::Unhex(input.substr(tagpos+1),supplied)||supplied.size()!=32)return false;
    const auto mac=security::Hmac(key,security::Bytes(body.begin(),body.end()));
    if(!security::Same(mac.data(),supplied.data(),mac.size()))return false;
    nonce=candidate;return true;
}
} // namespace kick_protocol

// An alternative for local, room-host-authorized kick control: no pairing
// files or hardcoded shared secrets. The receiver MUST independently confirm
// that the sender was the Photon room's current MasterClient (server actor ID).
namespace kick_protocol {
constexpr std::string_view kHostMagic="GMMKCK2";
inline std::string EncodeHost(uint64_t nonce,int sender,int recipient){
    if(!nonce||sender<=0||recipient<=0)return {};
    return std::string(kHostMagic)+"|K|"+std::to_string(nonce)+"|"+std::to_string(sender)+"|"+std::to_string(recipient);
}
inline bool DecodeHost(const std::string& input,int attested_sender,int local_actor,uint64_t& nonce){
    nonce=0;
    if(input.size()>96||input.rfind("GMMKCK2|K|",0)!=0)return false;
    const size_t npos=input.find('|',10),apos=input.find('|',npos==std::string::npos?0:npos+1);
    if(npos==std::string::npos||apos==std::string::npos||input.find('|',apos+1)!=std::string::npos)return false;
    uint64_t candidate=0;int sender=0,recipient=0;
    const auto parse=[](std::string_view v,auto& value)->bool{
        if(v.empty()||v.size()>20)return false;
        const auto res=std::from_chars(v.data(),v.data()+v.size(),value);
        return res.ec==std::errc{}&&res.ptr==v.data()+v.size();
    };
    if(!parse(std::string_view(input).substr(10,npos-10),candidate)||!candidate||
       !parse(std::string_view(input).substr(npos+1,apos-npos-1),sender)||
       !parse(std::string_view(input).substr(apos+1),recipient)||
       sender!=attested_sender||recipient!=local_actor)return false;
    nonce=candidate;return true;
}
} // namespace kick_protocol
