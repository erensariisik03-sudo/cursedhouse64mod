#pragma once
#include "il2cpp_runtime.h"
namespace photo_transport {
#ifdef __ANDROID__
void Install(const il2cpp::Api& api);
void Pump(const il2cpp::Api& api,bool scene_loading);
#else
inline void Install(const il2cpp::Api&){}
inline void Pump(const il2cpp::Api&,bool){}
#endif
}
