#include "InterceptEntry.h"
#include "Interceptor.h"

// Initialize all interception metadata deterministically.
InterceptEntry::InterceptEntry(InterceptEntryType type, addr_t address)
    : id(0), type(type), routing(nullptr), patched_addr(0), patched_size(0),
      relocated_addr(0), relocated_size(0), origin_insns{}, origin_insn_size(0) {


  this->patched_addr = address;
  this->id = Interceptor::SharedInstance()->count();
}