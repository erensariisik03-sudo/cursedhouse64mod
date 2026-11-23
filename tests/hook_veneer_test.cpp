#include "hook_veneer.h"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
using namespace hook_veneer;
struct Image {
    uintptr_t base=0x1000;
    std::array<uint32_t,256> words{};
    static bool Read(uintptr_t at,void* out,size_t count,void* opaque) {
        auto& image=*static_cast<Image*>(opaque);
        if(at<image.base || at-image.base>sizeof(image.words) || count>sizeof(image.words)-(at-image.base))return false;
        std::memcpy(out,reinterpret_cast<const uint8_t*>(image.words.data())+(at-image.base),count);return true;
    }
};
int main(){
    Image image;image.words.fill(0xD503201F);
    image.words[0]=0x14000040;image.words[1]=0x1400005F;
    image.words[64]=0xA9BF7BFD;
    auto result=Resolve(0x1000,Image::Read,&image);
    assert(result&&result->address==0x1100&&result->hops==1);
    assert(Overlaps(0x1000,16,0x1004));
    assert(!Overlaps(result->address,16,0x1004));
    const uint32_t neighbor=image.words[1];
    const uint32_t patch[]={0x58000051,0xD61F0220,0x12345678,0x00000001};
    std::memcpy(&image.words[64],patch,sizeof(patch));
    assert(image.words[0]==0x14000040&&image.words[1]==neighbor);
    image.words[64]=0xA9BF7BFD;
    image.words[16]=0x17FFFFF0;
    result=Resolve(0x1040,Image::Read,&image);
    assert(result&&result->address==0x1100&&result->hops==2);
    for(uint32_t instruction:{0x94000040u,0x54000800u}) {
        image.words[0]=instruction;result=Resolve(0x1000,Image::Read,&image);
        assert(result&&result->address==0x1000&&result->hops==0);
    }
    image.words[0]=0x14000000;assert(!Resolve(0x1000,Image::Read,&image));
    image.words[0]=0x14000040;image.words[64]=0x17FFFFC0;
    assert(!Resolve(0x1000,Image::Read,&image));
    image.words[0]=0x14000100;assert(!Resolve(0x1000,Image::Read,&image));
    assert(!Resolve(0x1001,Image::Read,&image));
    assert(!Resolve(0,Image::Read,&image));
    assert(Overlaps(0x1100,16,0x110C));
    assert(!Overlaps(0x1100,16,0x1110));
    assert(!Overlaps(0x1100,16,0));
    std::cout<<"ARM64 observer: tail branches, cycle/unmapped guards and neighboring entry protection: PASS\n";
}
