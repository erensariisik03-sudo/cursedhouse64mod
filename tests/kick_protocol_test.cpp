#include "kick_protocol.h"
#include <cassert>
#include <string>
#include <iostream>
int main(){
    photo_transfer_security::Bytes key(32);
    for(size_t i=0;i<key.size();++i)key[i]=static_cast<uint8_t>(i+1);
    const auto msg=kick_protocol::Encode(123456789ULL,3,7,key);
    uint64_t nonce=0;
    assert(kick_protocol::Decode(msg,3,7,key,nonce)&&nonce==123456789ULL);
    assert(!kick_protocol::Decode(msg,4,7,key,nonce));
    assert(!kick_protocol::Decode(msg,3,8,key,nonce));
    auto other=key;other[5]^=1;
    assert(!kick_protocol::Decode(msg,3,7,other,nonce));
    auto tampered=msg;tampered.back()=(tampered.back()=='0'?'1':'0');
    assert(!kick_protocol::Decode(tampered,3,7,key,nonce));
    assert(!kick_protocol::Decode("GMMKCK1|K|1|3|7|x",3,7,key,nonce));
    std::cout<<"kick message signature and actor binding: PASS\n";
}
