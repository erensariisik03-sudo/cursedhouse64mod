# GMM GitHub Manuel Kontrol + Fotoğraf/Kickle Güncellemesi

## GitHub Kontrol

- Depo: https://github.com/erensariisik03-sudo/-ekup-reposu
- Menü: AI > Geliştirici (şifre) > GitHub Kontrol > Repo durumunu kontrol et
- HTTP 200: PUBLIC
- HTTP 404: PRIVATE / ERİŞİLEMİYOR (özel, silinmiş veya yetki dışı olabilir)
- HTTP 403/429/5xx, zaman aşımı veya çevrimdışı: BAĞLANTI HATASI; private diye yorumlanmaz
- Yalnızca kullanıcı düğmeye bastığında HTTPS isteği atılır; uygulama donmasın diye arka planda çalışır
- Otomatik 30 saniyelik sorgu, modun uzaktan kapanması veya APK değiştirme BU SÜRÜMDE YOK

## Fotoğraf gönderimi

- Sahibin cihazında paylaşım başlatıcı **ayrı bir oyun menüsü yoktur**
- Alıcı Geliştirici > GMM Connect'ten fotoğraf listesi istediğinde gönderen cihazda küçük **Evet / Hayır** penceresi belirir
- Evet sonrası **sadece bu oyuncunun bu odadaki fotoğraf paylaşımı oturumu** (15 dakika) için bir kez onay yeterlidir. Aynı oturumda çoklu indirmede yeniden soru çıkmaz
- Ret veya 45 saniye cevap verilmemesi, isteği iptal eder. Oda değişince/oyuncu ayrılınca erişim sıfırlanır
- `src/gmm_photo_sender.cpp` yalnızca gönderici tarafının doğrudan `/sdcard/Pictures` klasörünü listeler ve seçilmiş JPG/PNG görselleri güvenli biçimde okur (8 MB/ dosya üst sınırı). Bütün Photon/RPC protokolü mevcut `src/photo_transport.cpp`, Evet/Hayır çizimi mevcut `src/keyboard_ui.cpp` üzerinden devam eder. **Tamamı bağımsız tek dosya değildir.**
- Android'in sistem depolama yetkisi ayrıca gereklidir; GMM oturum onayı bunun yerine geçmez

## Kickle

- `/sdcard/gmmod/admin_key.hex` artık **gerekli değildir**.
- Geliştirici kilidi açıldıktan sonra **sadece odanın Photon MasterClient oyuncusu** seçili mod kullanıcısına kick komutu yollayabilir
- Alıcı Photon EventData'daki doğrulanmış gönderici aktör kimliğini odanın MasterClient aktörüyle karşılaştırır; eşleşmezse işlem yok. Her kick için hedefe onay sorulmaz
- Hedef `PhotonNetwork.LeaveRoom(false)` işlemini dener; başarılıysa `Admin sizi banladı` bildirimi çıkar. Bu kalıcı bir ban değildir
- Oyun sahnesinden lobiye geçiş oyunun kendi Photon callback akışına bağlıdır; iki cihazda doğrulanmalıdır
- MasterClient olmayan bir geliştirici oyuncu kick gönderemez. İstenirse ileride açık anahtarlı ayrı admin rolü tasarlanabilir

## Derleme ve test

- Değişen veya eklenen C++ modüllerini yeni `.so` içine almak için GitHub Actions ile ARM64 derleyin; iki cihazda aynı sürüm olmalı
- Bu paket kod ve yerel test içerir, iki fiziksel cihazda uçtan uca doğrulanmış değildir
- DEX/APK değiştirilmedi. APK internete çıkabilmeli ve depolama izni manifestte bulunmalıdır
- Loglar: `adb logcat -d -s multiplayermod:I | Select-String "GMM photo|GMM kick"`
