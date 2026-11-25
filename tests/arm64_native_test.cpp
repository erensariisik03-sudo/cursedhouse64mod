#include "username_gate.h"
#include "hook_veneer.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>
static std::vector<uint32_t> Words(const char* path){
    std::ifstream input(path,std::ios::binary);assert(input.good());
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
    assert(!bytes.empty()&&bytes.size()%4==0);
    std::vector<uint32_t> words(bytes.size()/4);std::memcpy(words.data(),bytes.data(),bytes.size());return words;
}
struct Memory {
    std::map<uintptr_t,uint32_t> words;
    static bool Read(uintptr_t address,void* out,size_t count,void* context){
        auto& m=*static_cast<Memory*>(context);
        auto* dest=static_cast<unsigned char*>(out);
        for(size_t i=0;i<count;++i){
            const auto base=(address+i)&~uintptr_t{3};const auto it=m.words.find(base);
            if(it==m.words.end())return false;
            dest[i]=static_cast<unsigned char>(it->second>>(((address+i)&3)*8));
        }
        return true;
    }
};
static void Observer(const char* file,size_t footprint){
    std::ifstream input(file);assert(input.good());uintptr_t entry,target,next_api;
    input>>std::hex>>entry>>target>>next_api;
    Memory mem;uintptr_t a;uint32_t w;while(input>>std::hex>>a>>w)mem.words[a]=w;
    const auto found=hook_veneer::Resolve(entry,Memory::Read,&mem);
    assert(found&&found->address==target&&found->hops==1);
    assert(hook_veneer::Overlaps(entry,footprint,next_api)); // Old stub patch unsafe.
    assert(!hook_veneer::Overlaps(target,footprint,next_api));
    unsigned char bytes[16];assert(Memory::Read(target,bytes,footprint,&mem));
}
int main(){
    auto a64=Words("tests/fixtures/username_arm64.bin");
    auto gate=username_gate::Find(a64.data(),a64.size());
    assert(gate&&gate->compare==73&&gate->string_register==21&&gate->length_register==8);
    // The lower signed comparison is retained before the instrumented unsigned upper comparison.
    assert(a64[70]==0xB94012A8&&a64[71]==0x7100091F&&a64[72]==0x540010CD);
    assert(a64[73]==0x7100691F&&a64[74]==0x540017E2);
    for(const auto& mutation:{std::pair<size_t,uint32_t>{70,0xF9400AA8}, // wrong-width load
                            {71,0x7100051F}, // wrong minimum
                            {72,0x540010C0}, // wrong lower branch condition
                            {73,0x7100693F}, // different length register
                            {74,0x540017E1}}){
        auto wrong=a64;wrong[mutation.first]=mutation.second;
        assert(!username_gate::Find(wrong.data(),wrong.size()));
    }
    // Two plausible copies remain ambiguous even with the new shared-load form.
    std::vector<uint32_t> ambiguous=a64;ambiguous.insert(ambiguous.end(),a64.begin(),a64.end());
    assert(!username_gate::Find(ambiguous.data(),ambiguous.size()));
    Observer("tests/fixtures/observer_arm64.txt",16);
    std::cout<<"Actual ARM64 native fixtures: username upper check, preserved minimum, malformed/ambiguous rejection and safe runtime-invoke veneer target: PASS\n";
}
