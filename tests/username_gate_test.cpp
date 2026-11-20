#include "username_gate.h"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
using namespace username_gate;
int main() {
    std::array<uint32_t,128> a64{}; a64.fill(0xD503201F);
    a64[6]=0xB9401269;a64[7]=0x7100093F;a64[8]=0x54000A4D;
    a64[13]=0xB9401269;a64[14]=0x7100693F;a64[15]=0x54000AAA;
    auto gate=Find(a64.data(),a64.size());
    assert(gate && gate->compare==14 && gate->string_register==19 && gate->length_register==9);
    a64[13]=0xB9400E69; assert(!Find(a64.data(),a64.size())); a64[13]=0xB9401269;
    a64[14]=0xF100693F; assert(!Find(a64.data(),a64.size()));
    std::vector<uint16_t> ascii(1025,'a');
    assert(ComparisonValue(25,ascii.data())==25); assert(ComparisonValue(26,ascii.data())==25);
    assert(ComparisonValue(1024,ascii.data())==25); assert(ComparisonValue(1025,ascii.data())==1025);
    std::vector<uint16_t> emoji;
    for(int i=0;i<1024;++i) {emoji.push_back(0xD83D);emoji.push_back(0xDE00);}
    assert(ScalarCount(emoji.data(),emoji.size(),1024)==1024);
    assert(ComparisonValue(2048,emoji.data())==25);
    emoji.push_back('x'); assert(ComparisonValue(2049,emoji.data())==2049);
    assert(ComparisonValue(30,nullptr)==30);
    std::cout<<"Username gate: ARM64 shapes, ambiguous/missing patterns rejected, 25/26/1024/1025 and UTF-16 emoji boundaries: PASS\n";
}
