#include "../src/photo_transfer_security.h"
#include <cassert>
#include <iostream>
using namespace photo_transfer_security;
int main(){
 const auto sha=Hash(Bytes{'a','b','c'});assert(Hex(sha.data(),32)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 const Bytes salt={'s','a','l','t'};const auto k=Pbkdf2("password",salt,1);assert(Hex(k.data(),32)=="120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
 Gate a;assert(!a.Create("short"));assert(a.Create("CorrectHorse123!"));const std::string saved=a.Save();
 assert(!a.SetReceiving(true));Gate b;assert(b.Load(saved));assert(!b.Receiving());assert(!b.Unlock("wrong"));assert(!b.SetReceiving(true));assert(b.Unlock("CorrectHorse123!"));assert(b.SetReceiving(true));assert(b.Receiving());b.Lock();assert(!b.Receiving());
 Gate c;assert(c.Load(saved));for(int i=0;i<5;++i)assert(!c.Unlock("wrong"));assert(!c.Unlock("CorrectHorse123!"));
 Bytes photo={0xff,0xd8};photo.resize(2045,0x43);photo[2043]=0xff;photo[2044]=0xd9;
 Offer offer;offer.id=77;offer.sender=1;offer.recipient=2;offer.total=photo.size();offer.chunks=(photo.size()+kChunkBytes-1)/kChunkBytes;offer.sha=Hash(photo);
 Reassembler r;assert(!r.Begin(offer,2,b));assert(!r.Begin(offer,5,b));assert(b.Unlock("CorrectHorse123!")&&b.SetReceiving(true));assert(r.Begin(offer,2,b));for(size_t i=0;i<offer.chunks;++i){size_t start=i*kChunkBytes,n=std::min(kChunkBytes,photo.size()-start);Bytes piece(photo.begin()+start,photo.begin()+start+n);assert(!r.Push(offer.id,3,i,piece));assert(r.Push(offer.id,1,i,piece));assert(!r.Push(offer.id,1,i,piece));}
 Bytes back;assert(r.Finish(back));assert(back==photo);assert(!r.Finish(back));
 r.Begin(offer,2,b);photo[8]^=1;offer.sha[0]^=1;assert(!r.Begin(Offer{77,1,2,kMaxImageBytes+1,1,{}},2,b));
 std::cout<<"GMM photo security gate, SHA256, PBKDF2 and bounded reassembly: PASS\n";
}
