# GMM — Klavye, Kickle ve yerel fotoğraf paylaşımı

## Bu sürümün kapsamı

- Sohbet alanında Android klavyesinin **Tamam** tuşu metni bir kere oyun sohbetine yollar ve klavyeyi kapatır. Metin `Enter text...` alanında kalır.
- Klavye dışına dokunmak yalnızca klavyeyi kapatır; mesaj gönderilmez ve metin silinmez. Oyunun Gönder düğmesine tekrar tekrar basmak aynı metni tekrar yollar. Oyunun/Photon'un normal sınırlamaları geçerli olabilir.
- **Oyun > Fotoğraf paylaşımı** menüsü kaldırıldı. Alıcı tarafındaki **Geliştirici > GMM Connect** korunuyor.
- Yeni **Geliştirici > Kickle** ekranı, seçilen oda oyuncusunun eşleştirilmiş mod istemcisine imzalı Photon olayı göndermeyi dener. Hedef istemci de aynı yönetici anahtarıyla yerel olarak yapılandırılmışsa `PhotonNetwork.LeaveRoom(false)` çağrılır ve `Admin sizi banladı` Android Toast'u gösterilir. Bu kalıcı bir ban değil; kurulumdan kaldırma veya başka cihazların oyununu bozma yöntemi değil.

## Fotoğraf paylaşımına bir kez yerel izin verme (menüsüz)

Paylaşacak cihazın sahibi açıkça kendi cihazında aşağıdaki dosyayı hazırlamalıdır. Varsayılan durum **kapalıdır**; başka cihazdan gelen paketler yetkiyi açamaz. Her fotoğraf için yeni pencere gösterilmez.

Dosya: `/sdcard/gmmod/pictures_share.cfg`

İlk ve tek satır (ASCII):

```text
GMM_PICTURES_SHARE=ON
```

Windows PowerShell üzerinden kendi cihazına oluşturmak için:

```powershell
'GMM_PICTURES_SHARE=ON' | Set-Content -Encoding ascii pictures_share.cfg
adb shell mkdir -p /sdcard/gmmod
adb push pictures_share.cfg /sdcard/gmmod/pictures_share.cfg
```

Bu açık yetki yalnızca `Pictures` klasörünün **doğrudan içindeki** (alt klasörler hariç) geçerli JPG/PNG dosyaları için geçerlidir. Android'in Tüm dosyalara erişim izni de önceden verilmiş olmalıdır. Paylaşımı durdurmak için kendi cihazındaki `pictures_share.cfg` dosyasını kaldırın ya da ilk satırı değiştirin. Yeniden kontrol yaklaşık 2 saniyede yapılır.

## Kickle için kendi cihazlarını eşleştirme

**Kickle varsayılan olarak kapalıdır.** Hedef mod istemcisi `/sdcard/gmmod/admin_key.hex` dosyası bulunmadan herhangi bir uzaktan çıkış komutunu kabul etmez. Bu anahtarın iki cihazda da aynı olması gerekir. `YOBAZALİ03` sadece yönetici ekranının kilididir; ağ komutu anahtarı değildir.

Bir kez 32 rastgele baytlık ortak anahtar üretin (Windows PowerShell):

```powershell
$bytes = New-Object byte[] 32
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$rng.GetBytes($bytes)
$rng.Dispose()
$secret = [BitConverter]::ToString($bytes).Replace('-', '').ToLowerInvariant()
[System.IO.File]::WriteAllText((Join-Path (Get-Location) 'admin_key.hex'), $secret)
adb shell mkdir -p /sdcard/gmmod
adb push admin_key.hex /sdcard/gmmod/admin_key.hex
```

İkinci cihazda da **aynı** `admin_key.hex` dosyasını kullanın. Asla GitHub deposuna veya paylaşılacak ZIP'e bu anahtarı koymayın. Dosya paylaşılan depolamadadır; cihazdaki başka geniş yetkili uygulamalara karşı yüksek güvenlik garantisi vermez. Üretim sürümünde özel uygulama dizini/Android Keystore ile anahtar dağıtımı düşünülmelidir.

Bir odadayken **Geliştirici > Kickle**, oyuncuyu seçip `Seçilen oyuncuyu kickle` düğmesine basın. Hedefin mod sürümü de bu pakete güncellenmiş olmalı. Bunun çalışması Photon'un olay iletmesine ve oyunun `OnLeftRoom` arayüz davranışına bağlıdır. Lobide ve oyun sahnesinde **fiziksel test henüz yapılmadı**. Komut yalnızca odadan çıkma isteğidir; oyunun sahne geçişi otomatik olmuyorsa ek entegrasyon gerekir.

Tanı logları: `adb logcat -d -s multiplayermod:I | Select-String 'GMM kick|GMM photo'`

## Değişen dosyalar

- `src/unity_keyboard.cpp` — sohbet metni koruma / dış dokunma
- `src/keyboard_ui.cpp` — fotoğraf menüsünün kaldırılması, geliştirici Kickle sayfası
- `src/photo_transport.cpp` — menüsüz yerel paylaşım anahtarı ve aynı Photon kanalı üzerinden imzalı kick komutu
- `src/kick_bridge.h`, `src/kick_protocol.h` — Kickle köprüsü ve HMAC-SHA256 doğrulaması
- `tests/unity_editor_test.cpp`, `tests/keyboard_ui_test.cpp`, `tests/kick_protocol_test.cpp` — regresyon testleri

## Kapsam dışı

- DEX/APK modifikasyonu yapılmadı.
- Uzaktan izinsiz galeri taraması/otomatik açma yok.
- Gerçek ARM64 build ve iki cihaz üzerinde Photon kick doğrulaması yapılmadı.
