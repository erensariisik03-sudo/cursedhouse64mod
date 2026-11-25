#pragma once
#include "il2cpp_runtime.h"
namespace auto_join {
// Metadata çözülünce bir kez (Unity thread). Başarısız kanca ölümcül değildir.
void Install(const il2cpp::Api& api);
// Her karede Unity betik thread'inden: "Game full" sonrası düğme teklifi ve yeniden deneme döngüsü.
void Pump(const il2cpp::Api& api);
}
