#pragma once

#include <cstddef>
#include <cstdint>

// Minimal ARM32 absolute inline hook used only for armeabi-v7a.
// The target is in ARM mode (bit 0 must be clear).
int ArmHook(void* target, void* replacement, void** original);
