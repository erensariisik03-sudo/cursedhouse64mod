# GMM Connect — İlk fotoğraf aktarımı (ARM64 deneysel sürüm)

Bu kaynak, GMM'nin mevcut şifre kilidini **değiştirmeden** iki cihaz arasında
**yalnız kullanıcıların seçip kabul ettiği** küçük JPEG/PNG fotoğraf aktarımı
akışını ekler. Başka cihazın galerisini/klasörlerini uzaktan taramaz.

## Derleme ve APK

- GitHub Actions, `src/photo_transport.cpp` dosyasını yeni ARM64 `libmultiplayermod.so` içine derler.
- Çıkan `libmultiplayermod.so` dosyasını APK'nin `lib/arm64-v8a/` dizinine koyun.
- Önceki SEDA/ONNX kütüphaneleri ve `assets/gmm_seda` düzeni olduğu gibi kalır.
- Her iki telefonda da **aynı yeni mod sürümü** kurulu ve **aynı Photon odası** açık olmalı.
- `MANAGE_EXTERNAL_STORAGE` izni önceki manifestte mevcut olabilir, ancak otomatik verilmez.
  İlk açılış DEX ekranı bu pakette değiştirilmedi. İzin açık değilse gönderen
  `/sdcard/Pictures/GMMShare` klasörünü okuyamayabilir.
- Kaynak paket **derlenmiş bir APK veya Android üzerinde doğrulanmış .so değildir.**

## İlk fotoğraf denemesi

1. **Gönderen cihazda** `/sdcard/Pictures/GMMShare/` klasörünü oluşturup
   **256 KiB'den küçük** bir JPEG ya da PNG koyun. Büyük telefon fotoğraflarını
   önce küçültün. Bilgisayardan ADB ile örnek:

   ```powershell
   adb shell mkdir -p /sdcard/Pictures/GMMShare
   adb push "C:\fotograflar\deneme.jpg" /sdcard/Pictures/GMMShare/
   ```

2. **Her iki cihazda** GMM > Genel AI > Geliştirici bölümüne girip mevcut yerel
   alıcı şifresini doğrulayın. Alıcı cihaz **Dosya Alma: AÇIK** durumuna gelsin.
3. Gönderende **Fotoğrafları / oyuncuları yenile** düğmesine basın. Hedef
   oyuncuyu ve göndermek istediğiniz **yerel** JPEG/PNG dosyasını seçin.
4. **Seçilen fotoğrafı teklif et** düğmesine basın.
5. Alıcıda teklif ve hedef oyuncunun adı görünür; **Fotoğrafı kabul et** düğmesi
   ile onay verir. Reddedilirse aktarım gerçekleşmez.
6. Fotoğraf 768 baytlık güvenilir Photon paketlerine ayrılır. Alıcı SHA-256
   bütünlük denetimini geçerse kaydeder.
7. Alıcıda geniş depolama izni varsa dosya `/sdcard/Pictures/GMMReceived/`
   dizinine, yoksa uygulamanın özel `files/GMMReceived/` dizinine yazılır.
   Galeri uygulamasında hemen görünmesi Android medya taramasına bağlıdır.

## Bilinen sınırlamalar

- 256 KiB fotoğraf sınırı: ilk ağ denemesi için kasıtlı olarak düşük tutuldu.
- Aynı Photon odası şarttır. Oyun event kodu **184** deneme için ayrıldı;
  gerçek oyunda başka bir özellik tarafından kullanılıp kullanılmadığı
  cihaz loguyla doğrulanmalıdır.
- Photon tarafında `SendOptions.Encrypt=true` seçilir. Bu **istemci-sunucu
  taşıma şifrelemesidir; RAR parolası ya da uçtan uca şifreleme değildir.**
- Uygulamanın kaynak dosyaları gizlenmedi/rastgele yeniden adlandırılmadı.
  Güvenlik, dosya adlarının gizlenmesine dayanmaz.
- Oyun içi gerçek ağ aktarımı **henüz iki ARM64 cihazda doğrulanmadı**.
  Önce `RaiseEvent`, `LoadBalancingClient.OnEvent`, `get_Item`,
  `SendOptions.SendReliable`, `EventData.Code` metadata bağlarının oyunun kendi
  sürümüyle eşleştiği loglarla doğrulanmalı.
- İlk açılışta izin isteme DEX değiştirilmedi; önceki izin menüsü kullanılabilir.
- Mevcut cihaz-başına alıcı parolası korunmuştur; herkese açık sabit şifre
  bu sürümde güvenlik anahtarı olarak gömülmedi.

## Testler

`tests/photo_transfer_protocol_test.cpp`, `tests/photo_transfer_security_test.cpp`,
`tests/keyboard_ui_test.cpp` ve `tests/startup_test.cpp` yerel testlerde PASS verdi.
Bu testler telefon/Photon sunucu testinin yerine geçmez.

Log toplamak için:

```powershell
adb logcat -c
# Oyunu başlat, alıcı/gönderici denemeyi yap
adb logcat -d -s multiplayermod:V > "$env:USERPROFILE\\GMM_photo_log.txt"
```

Güvenli kullanım: Fotoğrafı **gönderici seçer**, alıcı **şifresiyle alma modunu
etkinleştirir** ve **her transfer teklifini ayrıca kabul eder**. Uzaktan sessiz
klasör taraması, habersiz dosya alma veya gizli erişim eklenmemiştir.
