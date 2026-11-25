# Sesli AI / Myinstants güncellemesi

Taban: GMM_Stable_Sohbet_Cool_Gardirop_FIX.zip. Klonlama, zorluk, oda limiti ve gardırop kaynakları değiştirilmedi.

- Android yerel TTS: robotik DSP kaldırıldı. 50 doğal ton, 0 daha kalın, 100 daha ince. Türkçe çevrimdışı sesler menüde listelenir; ses adı erkek olduğunu belirtiyorsa otomatik tercih edilir. Android Voice API cinsiyet alanı sunmaz; diğer sesler kullanıcı tarafından denenebilir. Ses motoru açılışta önceden hazırlanır.
- SEDA/ASR, Gemini ve TTS bağımsız işçiler. Tam cümleler API kuyruğuna girer; ekrandaki kısmi tanıma sadece UI içindir. ASR normal otomatik modda AI/TTS/oynatma sırasında da seçili uzak konuşmacıyı dinler. Manuel kelime seçimi modunda onay beklenir. Kuyruklar sınırlıdır, aşırı yoğun konuşmada eski bekleyen işler düşebilir. Oda değişimi/stop eski işleri geçersiz kılar.
- Gemini cevabı hazır olur olmaz sohbet kuyruğuna ve ayrı TTS kuyruğuna alınır; sohbet manuel taslak/oyun gönderim kapısı nedeniyle gecikebilir. Sesli istekler 3 model, 256 token, 6 geçmiş kaydı, 3.5 s bağlantı ve 8 s okuma zaman aşımı. TTS 12 s bekleme sınırı. Aşamaların süreleri logcat'te ölçülür.
- Myinstants: sesli AI > Myinstants sesleri > ara > bir sonuç seç. Arama doğrudan sitenin HTML çıktısından (resmi sabit API değil); HTML değişirse sonuç dönmeyebilir. Sesli AI açık, sesli odaya bağlı ve oda hedefi seçili olmalıdır. MP3 Android MediaExtractor/MediaCodec ile PCM'e çevrilir. 4 MiB indirme, 15 s oynatma sınırı, %65 PCM ölçeği. Oda dışına veya mikrofona geçiş yapılmaz, gerçek mikrofon mevcut clip lease mekanizmasıyla izole edilir. Oda değişiminde indirme sonucu atılır. Site aramasına yalnız yazılan sorgu iletilir.

## Test durumu
Masaüstü C++: voice_ai_core_test, voice_ai_runtime_test, keyboard_ui_test başarılı. JNI HTTP masaüstü başlıkları ile syntax kontrolü ve Android koşullu voice_ai.cpp syntax kontrolü başarılı. Android NDK/gerçek APK/cihaz, MediaCodec, canlı Myinstants ağ erişimi ve Photon ses teslimi test edilmedi. ARM64 CMake hedefi korunuyor; yeni sistem Android mediandk kütüphanesine bağlanır.

```powershell
adb logcat -v threadtime -s multiplayermod:I | Select-String "GMM_DIAG voice|GMM_DIAG tts"
```
