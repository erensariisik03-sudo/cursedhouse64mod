#pragma once

#include <cstddef>
#include <cstdint>

// Minimal AArch64 absolute inline hook used by the arm64-v8a build.
// The first 16 bytes at the target are replaced by:
//   ldr x16, #8
//   br  x16
//   <8-byte absolute target>
// A trampoline executes the saved first 16 bytes and jumps back to target+16.
int Arm64Hook(void* target, void* replacement, void** original);
