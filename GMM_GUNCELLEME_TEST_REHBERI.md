# GMM — Fotoğraf / sohbet / toast test notları

## Değişiklikler

- Açılışta çift bildirimin yerine **tek renkli oyun içi Toast görünümlü bildirim** gösterilir: Di (kırmızı), Cap (turuncu), rio Mod (sarı). Android'in sistem Toast'unu renklendirme iddiası yoktur.
- **Oyun > İsim/mesaj kopyalama**: oda adı, gerçek Photon takma adları, son 30 görüntülenen sohbet mesajı (rich-text etiketleri dahil) tek dokunuşla kopyalanabilir. Koplayalanan metin kırpılmış arayüz etiketinden değil ham dizgiden alınır (kaynakta güvenlik için mesaj başına 4096 bayta kadar tutulur).
- Sadece normal oyun sohbetinde: Android klavyesinin **Tamam/Done** eylemi TMP `OnSubmit` olayını oluşturursa oyun `ChatManager.SendMessage` yöntemi çağrılır; bir kez gönderip klavye kapanır. Kullanıcı başka bir noktaya gerçekten dokunarak sohbet alanından çıkarsa aynı akış tetiklenir. Şifre girişleri, programatik odak kayıpları ve oyun içi cooldown bu davranışa dahil edilmez. Gerçek IME varyasyonları cihazda test edilmelidir.
- Fotoğrafın karşı cihazdan gönderilmesine dair Photon altyapısı / karşı taraf onayı değiştirilmedi. Daha yavaş emülatörler için gelen paket tamponu büyütüldü, gönderim aralığı biraz yavaşlatıldı. **Alıcıda** Android 10+ MediaStore ile `Pictures/GMMReceived` içine kaydetme denenir. Başarısızsa mevcut geniş dosya izni ile normal klasör, ardından uygulamanın `files/GMMReceived` klasörü denenir. Gönderici habersiz dosya açamaz; karşı cihazın paylaşım onayı korunur.

## İki cihazı birlikte ayıklama (Windows PowerShell)

Önce cihaz kimliklerini bul:

```powershell
adb devices
```

Örnek emülatör: `emulator-5554`. Telefondaki gerçek kimliği aşağıdaki `<TELEFON_ID>` yerine yaz.

```powershell
adb -s emulator-5554 logcat -d -v time -s multiplayermod:I multiplayermod:E | Select-String 'GMM photo|GMM chat input'
adb -s <TELEFON_ID> logcat -d -v time -s multiplayermod:I multiplayermod:E | Select-String 'GMM photo|GMM chat input'
```

Gönderici telefonda `GMM photo owner: fetch received` ve `GMM photo TX:`; alıcı emülatörde `GMM photo RX:` ve `GMM photo receiver: done authorized=... verified=... saved=...` beklenir. `verified=0` ağ verisi / tamamlanma; `verified=1 saved=0` depolama / galeri kaydı sorununa işaret eder. MediaStore başarılıysa `GMM photo save: MediaStore success` görülür. Her iki cihazın aynı C++ sürümü ve aynı Photon odasında olması gerekir.

## Test kapsamı

Ana bilgisayarda C++ sözdizimi, Android JNI sözdizimi, Unity klavye davranışları, ImGui arayüzü ve fotoğraf protokolü birim testleri çalıştırıldı. **Gerçek ARM64 APK/.so derlemesi, Android sistem Toast/IME davranışı ve telefon–emülatör aktarım testi yapılmadı.** GitHub Actions'dan ARM64 derlemesi alıp ikisine de kurun. İlk oyuna girişte MANAGE_EXTERNAL_STORAGE izin ekranı önceki paket gibi kalır.
