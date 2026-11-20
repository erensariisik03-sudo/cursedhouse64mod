#pragma once
#include "il2cpp_runtime.h"
namespace chat_bot {
// Yalnız Unity'nin mevcut betik thread'inden (EventSystem.Update hook'u). Yeni hook yok.
// Seçilen oyuncunun sohbet satırlarını okur, Gemini'ye gönderir, cevabı oyunun kendi
// ChatManager.SendMessage akışıyla yollar. Kapalıyken yalnız oyuncu listesini okur.
void Pump(const il2cpp::Api& api);
}
