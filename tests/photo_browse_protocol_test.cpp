#include "../src/photo_browse_protocol.h"
#include <cassert>
#include <iostream>
using namespace photo_browse_protocol;
int main(){
 Packet p;
 for(const auto kind:{Kind::Request,Kind::Grant,Kind::Deny,Kind::End}){
  auto code=Encode(kind,123456789,8);assert(!code.empty()&&Decode(code,p)&&p.kind==kind&&p.id==123456789&&p.recipient==8);
 }
 for(const auto kind:{Kind::Listing,Kind::Fetch}){
  auto code=Encode(kind,42,8,u8"𝓕otoğraf 👑.png");
  assert(Decode(code,p)&&p.name==u8"𝓕otoğraf 👑.png"&&p.kind==kind);
 }
 for(const auto& bad:{"../photo.jpg","../../Android/data/a.png","foo\\bar.jpg",".hidden.png","secret.txt","x.png|GMMIMG2","","x.jpg\n"}){
  assert(!NameOk(bad));assert(Encode(Kind::Fetch,1,2,bad).empty());
 }
 for(const auto& bad:{"GMMIMG2|R|0|2","GMMIMG2|R|1|0","GMMIMG2|F|1|2|aa","GMMIMG2|G|1|2|garbage","GMMIMG2|X|-1|2","GMMIMG2|L|5|3|gg","GMMIMG2|R|5|3|x"})assert(!Decode(bad,p));
 std::cout<<"GMM consent-based photo listing codec: PASS\n";
}
