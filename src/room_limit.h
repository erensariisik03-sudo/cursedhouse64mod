#pragma once
#include "il2cpp_runtime.h"
namespace room_limit {
#ifdef __ANDROID__
void Pump(const il2cpp::Api& a,bool loading);
#else
inline void Pump(const il2cpp::Api&,bool){}
#endif
}
