# GMM Genel AI Menüsü — 24 Kasım 2026

Bu sürüm, önceki SEDA APK-assets modeli ve SetVM düzeltmesinin üzerine hazırlanmıştır.

## Yeni menü

- **Genel AI:** Cool Modu, API Anahtarı, Metin AI, Sesli AI, Kişilik, Oyuncu mesajı, Geçmiş.
- **Metin AI:** Kişi seçimi, herkese cevap, adla çağırma, mesaj filtresi, yanıt uzunluğu, hafıza sıfırlama.
- **Sesli AI:** Aynı anda tek Photon ses konuşmacısı; otomatik veya listeden actor seçimi; isim filtresi yoktur. Manuel kelime seçimi açıkken SEDA'nın tanıdığı kelimeler butonlarda seçilir, ancak onaydan sonra AI isteği yapılır.
- **Kişilik ve API anahtarı:** Her iki AI için ortak konfigürasyon. Sesli ve Metin AI birbirini devre dışı bırakır.
- **Cool Modu:** Kapalıyken AI aç/kapat, kişilik/model değişimi için sohbet terminali animasyonu susar. Açıkken Sesli AI açılış/kapanışında da animasyon görünür.
- **Sesli AI yanıtları:** Başarılı üretilen sesli yanıtın metni de oyun sohbeti kuyruğuna eklenir; mesajın gerçekten gönderilmesi mevcut sohbetin hazır/odak/cooldown şartlarına tabidir.
- **Oda adı:** Oda adını kopyala düğmesi, İsim Kopyala bölümüne eklendi.
- **Oyuncu mesajı:** Odadaki kişi listesinden seçilen isim geçici sohbet takma adı olarak kullanılabilir; bu, gerçek Photon aktör/hesap kimliğini değiştirmez. Gönderme, odada bulunma, sohbet kısıtları, taslak/odak koruması ve 20 s kuyruk süresine tabidir.

## Derleme / yükleme

GitHub'a kaynak ZIP'teki **src/** ve **tests/** değişikliklerini aktarın. Mevcut `.github` workflow ARM64 modül derler. Yeni `libmultiplayermod.so` dosyası APK'deki `lib/arm64-v8a` konumuna eklenmelidir. SEDA dosyaları APK'de `assets/gmm_seda` altında kalmalıdır. Bu çalışma `classes8.dex` veya `AndroidManifest.xml` üzerinde değişiklik yapmaz.

## Test durumu

Linux ortamında UI klavye testi, startup, ses çekirdek/test mikrofonu, metin AI testleri ve özel Cool/isim takma adı regresyonları başarıyla çalıştırılmıştır. Tam `tests/run.sh` koşusu süre sınırı nedeniyle tamamlanmadı; Android ARM64 NDK derlemesi ve gerçek cihaz/oda testleri bu ortamda yapılmadı. Özellikle diğer oyuncuların geçici adı nasıl gördüğü oyunda doğrulanmalıdır.
