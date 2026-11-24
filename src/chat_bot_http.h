#pragma once
#include <string>
#include <jni.h>
namespace chat_bot_http {
// Android panosundaki düz metin (ön plandaki uygulama olarak). Başarısızsa false.
bool ReadClipboard(std::string& out);
bool WriteClipboard(const std::string& text); // copies plain text (UTF-8) to the Android clipboard
void SetVM(JavaVM* vm);
// Arka plan thread'inde çağrılır (Unity thread'inde değil). code: HTTP durumu, 0 = bağlantı hatası.
bool Post(const std::string& url, const std::string& api_key, const std::string& body,
          int& code, std::string& response);
// Lightweight HTTPS GET; returns true if an HTTP response was received.
// Background thread only; code=0 means a connection or TLS error.
bool GetStatus(const std::string& url, int& code);
// Android sistem TTS (TextToSpeech) ile Türkçe, erkek tonlu WAV üretir. Arka plan thread'inde çağrılır.
bool Speak(const std::string& text, std::string& wav);
}
