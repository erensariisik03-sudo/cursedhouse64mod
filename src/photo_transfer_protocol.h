#pragma once
// Explicit, user-initiated GMM-to-GMM image offer transport.
// This is an application protocol, not cryptography: the Photon SDK must
// encrypt the underlying event transport; SHA-256 only detects corruption.
#include "photo_transfer_security.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace photo_transfer_protocol {
using photo_transfer_security::Bytes;
constexpr uint8_t kEventCode = 184; // reserved for this mod, confirm game doesn't use it
constexpr std::string_view kMagic = "GMMIMG1";
enum class Kind { Invalid, Offer, Accept, Chunk, Done, Cancel };
struct Packet {
    Kind kind = Kind::Invalid;
    uint64_t id = 0;
    int recipient = 0;
    size_t total = 0, index = 0;
    std::array<uint8_t,32> digest{};
    Bytes bytes;
};
inline bool Number(std::string_view s, uint64_t& result) {
    if (s.empty() || s.size() > 20 || s.front() == '-' || s.front() == '+') return false;
    const auto begin=s.data();const auto end=s.data()+s.size();
    auto r=std::from_chars(begin,end,result,10);
    return r.ec==std::errc{} && r.ptr==end;
}
inline bool PositiveInt(std::string_view s, int& result) {
    uint64_t n=0;if(!Number(s,n)||n==0||n>2147483647u)return false;
    result=static_cast<int>(n);return true;
}
inline std::vector<std::string_view> Fields(std::string_view source) {
    std::vector<std::string_view> fields;
    if (source.size()>1800) return fields;
    size_t from=0;
    for (;;) {const size_t p=source.find('|',from);fields.push_back(source.substr(from,p==std::string_view::npos?p:p-from));
        if (fields.size()>9) return {};
        if (p==std::string_view::npos) break;
        from=p+1;
    }
    return fields;
}
inline bool Decode(std::string_view input,Packet& out) {
    out={};const auto f=Fields(input);
    if(f.size()<3||f[0]!=kMagic)return false;
    if(!Number(f[2],out.id)||!out.id)return false;
    if(f[1]=="O") {
        if(f.size()!=6||!PositiveInt(f[3],out.recipient))return false;
        uint64_t size=0;if(!Number(f[4],size)||size<8||size>photo_transfer_security::kMaxImageBytes||f[5].size()!=64)return false;
        Bytes sha;if(!photo_transfer_security::Unhex(std::string(f[5]),sha)||sha.size()!=32)return false;
        out.total=static_cast<size_t>(size);std::copy(sha.begin(),sha.end(),out.digest.begin());out.kind=Kind::Offer;return true;
    }
    if(f[1]=="A"){
        if(f.size()!=4||!PositiveInt(f[3],out.recipient))return false;
        out.kind=Kind::Accept;return true;
    }
    if(f[1]=="C"){
        uint64_t index=0;if(f.size()!=5||!Number(f[3],index)||index>=photo_transfer_security::kMaxChunks)return false;
        if(f[4].size()<2||f[4].size()>photo_transfer_security::kChunkBytes*2)return false;
        if(!photo_transfer_security::Unhex(std::string(f[4]),out.bytes)||out.bytes.empty())return false;
        out.index=static_cast<size_t>(index);out.kind=Kind::Chunk;return true;
    }
    if(f[1]=="D"&&f.size()==3){out.kind=Kind::Done;return true;}
    if(f[1]=="X"&&f.size()==3){out.kind=Kind::Cancel;return true;}
    return false;
}
inline std::string EncodeOffer(uint64_t id,int recipient,const Bytes& image) {
    if(!id||recipient<=0||!photo_transfer_security::IsImage(image))return {};
    const auto hash=photo_transfer_security::Hash(image);
    return std::string(kMagic)+"|O|"+std::to_string(id)+"|"+std::to_string(recipient)+"|"+
      std::to_string(image.size())+"|"+photo_transfer_security::Hex(hash.data(),hash.size());
}
inline std::string EncodeAccept(uint64_t id,int recipient) {
    return id&&recipient>0?std::string(kMagic)+"|A|"+std::to_string(id)+"|"+std::to_string(recipient):"";
}
inline std::string EncodeChunk(uint64_t id,size_t index,const Bytes& image) {
    const size_t from=index*photo_transfer_security::kChunkBytes;
    if(!id||index>=photo_transfer_security::kMaxChunks||from>=image.size()||image.size()>photo_transfer_security::kMaxImageBytes)return {};
    const size_t count=std::min(photo_transfer_security::kChunkBytes,image.size()-from);
    return std::string(kMagic)+"|C|"+std::to_string(id)+"|"+std::to_string(index)+"|"+
       photo_transfer_security::Hex(image.data()+from,count);
}
inline std::string EncodeDone(uint64_t id) {return id?std::string(kMagic)+"|D|"+std::to_string(id):"";}
inline std::string EncodeCancel(uint64_t id) {return id?std::string(kMagic)+"|X|"+std::to_string(id):"";}
}
