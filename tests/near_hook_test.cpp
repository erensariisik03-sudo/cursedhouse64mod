#include "InterceptRouting/Routing/FunctionInlineHook/NearOnlyHookRouting.h"
#include "Interceptor.h"
#include <cassert>
#include <cstring>
#include <iostream>
static int relocations=0,patches=0;static bool allocation=true,relocation_ok=true,patch_ok=true;static unsigned relocated_size=4;
static CodeBufferBase* emitted=nullptr;
Interceptor* Interceptor::SharedInstance(){static Interceptor instance;return &instance;}int Interceptor::count(){return 0;}
void InterceptRouting::Prepare(){}void InterceptRouting::Active(){}
bool InterceptRouting::GenerateRelocatedCode(){++relocations;entry_->origin_insn_size=relocated_size;entry_->relocated_addr=0x1234;return relocation_ok;}
CodeBufferBase* GenerateNearTrampolineBuffer(InterceptRouting*,addr_t,addr_t){return allocation?emitted:nullptr;}
extern "C" int DobbyCodePatch(void* address,uint8_t* bytes,uint32_t size){++patches;assert(size==4);if(!patch_ok)return -1;std::memcpy(address,bytes,size);return 0;}
int main(){
 uint32_t entries[]={0xF9401400,0xD65F03C0,0xF9401800,0xD65F03C0};CodeBufferBase branch;branch.Emit32(0x14000010);emitted=&branch;
 InterceptEntry entry(kFunctionInlineHook,reinterpret_cast<addr_t>(entries));NearOnlyHookRouting route(&entry);
 assert(route.Build(0x4567)&&relocations==1&&route.Patch()&&patches==1);
 assert(entries[0]==0x14000010&&entries[1]==0xD65F03C0&&entries[2]==0xF9401800&&entries[3]==0xD65F03C0);
 allocation=false;assert(!route.Build(0x4567)&&relocations==1&&patches==1);allocation=true;
 CodeBufferBase absolute;absolute.Emit64(0);absolute.Emit64(0);emitted=&absolute;assert(!route.Build(0x4567)&&relocations==1);emitted=&branch;
 relocation_ok=false;assert(!route.Build(0x4567));relocation_ok=true;relocated_size=8;assert(!route.Build(0x4567));relocated_size=4;
 assert(route.Build(0x4567));patch_ok=false;assert(!route.Patch());
 std::cout<<"Near-only getter hook: four-byte patch preserves adjacent getter/return, no absolute fallback, allocation/relocation/patch failures rejected: PASS\n";
}
