#pragma once
#include "il2cpp_runtime.h"
#include <cstdint>
namespace emoji_support {
// Invoke only on Unity's scripting thread, for the current text field.
uint64_t Probe(il2cpp::Api& api, il2cpp::Object* field, bool& query_ok);
}
