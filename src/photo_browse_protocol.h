#pragma once
// GMM IMG2 consent-based, one-session browse protocol. The data owner must
// explicitly approve a request before any filenames from the approved Pictures directory are sent.
// The attested sender must always come from Photon EventData, not the payload.
#include "photo_transfer_security.h"
#include <charconv>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace photo_browse_protocol {
constexpr std::string_view kMagic="GMMIMG2";
enum class Kind {Invalid, Request, Grant, Deny, Listing, End, Fetch};
struct Packet { Kind kind=Kind::Invalid; uint64_t id=0; int recipient=0; std::string name; };
inline bool Number(std::string_view input,uint64_t& value){
 if(input.empty()||input.size()>20||input.front()=='-'||input.front()=='+')return false;
 auto r=std::from_chars(input.data(),input.data()+input.size(),value);return r.ec==std::errc{}&&r.ptr==input.data()+input.size();
}
inline bool NameOk(std::string_view s){
 if(s.empty()||s.size()>128||s=="."||s==".."||s.front()=='.'||s.find('/')!=s.npos||s.find('\\')!=s.npos)return false;
 for(unsigned char c:s)if(c==0||c<32||c==127)return false;
 const auto dot=s.find_last_of('.');if(dot==s.npos)return false;
 std::string ext(s.substr(dot));for(char& c:ext)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
 return ext==".jpg"||ext==".jpeg"||ext==".png";
}
inline std::string Encode(Kind type,uint64_t id,int recipient,std::string_view name={}){
 if(!id||recipient<=0)return {};
 char kind='?';switch(type){case Kind::Request:kind='R';break;case Kind::Grant:kind='G';break;case Kind::Deny:kind='X';break;case Kind::Listing:kind='L';break;case Kind::End:kind='E';break;case Kind::Fetch:kind='F';break;default:return {};}
 const bool carries=type==Kind::Listing||type==Kind::Fetch;
 if(carries&&!NameOk(name))return {};
 if(!carries&&!name.empty())return {};
 std::string s=std::string(kMagic)+"|"+kind+"|"+std::to_string(id)+"|"+std::to_string(recipient);
 if(carries)s+="|"+photo_transfer_security::Hex(reinterpret_cast<const uint8_t*>(name.data()),name.size());
 return s;
}
inline bool Decode(std::string_view s,Packet& p){
 p={};if(s.size()>800)return false;
 std::vector<std::string_view> f;size_t start=0;
 while(start<=s.size()){
  auto end=s.find('|',start);f.push_back(s.substr(start,end==s.npos?s.npos:end-start));
  if(f.size()>5)return false;
  if(end==s.npos)break;
  start=end+1;
 }
 if(f.size()<4||f[0]!=kMagic||f[1].size()!=1)return false;
 if(!Number(f[2],p.id)||!p.id)return false;
 uint64_t to=0;if(!Number(f[3],to)||to==0||to>2147483647ULL)return false;p.recipient=static_cast<int>(to);
 switch(f[1][0]){case 'R':p.kind=Kind::Request;break;case 'G':p.kind=Kind::Grant;break;case 'X':p.kind=Kind::Deny;break;case 'L':p.kind=Kind::Listing;break;case 'E':p.kind=Kind::End;break;case 'F':p.kind=Kind::Fetch;break;default:return false;}
 const bool carries=p.kind==Kind::Listing||p.kind==Kind::Fetch;
 if(f.size()!=(carries?5U:4U))return false;
 if(carries){photo_transfer_security::Bytes b;if(!photo_transfer_security::Unhex(std::string(f[4]),b))return false;p.name.assign(b.begin(),b.end());if(!NameOk(p.name))return false;}
 return true;
}
} // namespace photo_browse_protocol
