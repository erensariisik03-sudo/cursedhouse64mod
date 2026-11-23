#include "Interceptor.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <new>
Interceptor* Interceptor::SharedInstance() { static Interceptor instance; return &instance; }
int Interceptor::count() { return 0; }
int main() {
 for(unsigned char poison:{0u,1u,0xA5u,0xFFu}) for(addr_t address:{addr_t{0x1000},addr_t{0x1001},addr_t{0x12345678},addr_t{0x12345679}}) {
  alignas(InterceptEntry) unsigned char storage[sizeof(InterceptEntry)];std::memset(storage,poison,sizeof(storage));
  auto* entry=new(storage) InterceptEntry(kFunctionInlineHook,address);
  assert(entry->patched_addr==address);
  assert(entry->routing==nullptr && entry->patched_size==0 && entry->relocated_addr==0 && entry->origin_insn_size==0);
  entry->~InterceptEntry();
 }
 std::cout<<"Real Dobby constructor: poisoned allocation, preserved address and deterministic metadata: PASS\n";
}
