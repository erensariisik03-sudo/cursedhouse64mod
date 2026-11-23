#pragma once
#include "dobby/dobby_internal.h"
#include "InterceptRouting/InterceptRouting.h"
extern CodeBufferBase *GenerateNearTrampolineBuffer(InterceptRouting *, addr_t, addr_t);
class NearOnlyHookRouting final : public InterceptRouting {
public:
  explicit NearOnlyHookRouting(InterceptEntry *entry) : InterceptRouting(entry) {}
  void DispatchRouting() override {}
  bool Build(addr_t target) {
    SetTrampolineTarget(target);
    auto *buffer = GenerateNearTrampolineBuffer(this, entry_->patched_addr, target);
    if (!buffer || buffer->GetBufferSize() != 4) return false;
    SetTrampolineBuffer(buffer);
    return GenerateRelocatedCode() && entry_->origin_insn_size == 4 && entry_->relocated_addr;
  }
  bool Patch() {
    return DobbyCodePatch((void *)entry_->patched_addr, GetTrampolineBuffer()->GetBuffer(), 4) == 0;
  }
};
