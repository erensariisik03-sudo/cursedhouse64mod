#include "../src/photo_transfer_protocol.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace photo_transfer_security;
using namespace photo_transfer_protocol;
int main() {
    Bytes jpeg(2301,0x48);jpeg[0]=0xff;jpeg[1]=0xd8;jpeg[jpeg.size()-2]=0xff;jpeg.back()=0xd9;
    Packet p;const auto offered=EncodeOffer(9223372036854775807ULL,17,jpeg);
    assert(Decode(offered,p));assert(p.kind==Kind::Offer&&p.total==jpeg.size()&&p.recipient==17);
    assert(p.id==9223372036854775807ULL);assert(p.digest==Hash(jpeg));
    assert(Decode(EncodeAccept(p.id,1),p)&&p.kind==Kind::Accept&&p.recipient==1);
    assert(Decode(EncodeChunk(45,1,jpeg),p)&&p.kind==Kind::Chunk&&p.index==1&&p.bytes.size()==768);
    assert(Decode(EncodeDone(45),p)&&p.kind==Kind::Done);
    assert(Decode(EncodeCancel(45),p)&&p.kind==Kind::Cancel);
    for (const auto& malformed : {"GMMIMG1|O|0|5|100|ab", "GMMIMG1|O|4|5|999999999|ab",
          "GMMIMG1|A|4|-1", "GMMIMG1|C|4|0|gh", "GMMIMG1|C|4|1|", "GMMIMG1|D|3|bad",
          "GMMIMG1|A|3|0", "GMMIMG1|O|4|5|234|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaz"})
        assert(!Decode(malformed,p));
    Gate gate;assert(gate.Create("long-password"));assert(gate.Unlock("long-password"));
    assert(gate.SetReceiving(true));
    const auto accepted=EncodeOffer(545,1,jpeg);
    assert(Decode(accepted,p));
    Offer offer;offer.id=p.id;offer.sender=4;offer.recipient=p.recipient;
    offer.total=p.total;offer.chunks=(p.total+kChunkBytes-1)/kChunkBytes;offer.sha=p.digest;
    Reassembler reasm;assert(!reasm.BeginTrusted(offer,1,false));assert(reasm.Begin(offer,1,gate));
    for(size_t i=0;i<offer.chunks;i++){
        Packet c;assert(Decode(EncodeChunk(545,i,jpeg),c));
        assert(!reasm.Push(c.id,77,c.index,c.bytes)); // sender must come from Photon
        assert(reasm.Push(c.id,4,c.index,c.bytes));
    }
    Bytes output;assert(reasm.Finish(output)&&output==jpeg);
    std::cout << "GMM user-approved photo offer, strict packet codec and integrity: PASS\n";
}
