# GMM Photon RaiseEventOptions kurucu düzeltmesi

## Logdan saptanan durum
- `GMM photo TX exception via=PhotonNetwork.RaiseEvent class=System.NullReferenceException`
- `room=1`, `peers=1`, `selected=1`: oda/oyuncu seçimi çalışıyor.
- `fallback=1` eski kodda yalnızca alternatif metodun bulunabildiğini ifade ediyordu.

## Kaynak düzeltmesi
- `il2cpp_object_new(RaiseEventOptions)` sonrasında yönetilen `RaiseEventOptions::.ctor()` çağrılıyor.
- `Flags` referansı kurucudan sonra kontrol ediliyor; boşsa gönderme güvenli biçimde durduruluyor.
- Fallback logları `fallback_available`, `fallback_attempted`, `fallback_invoked` olarak ayrıldı.
- Diğer mevcut GMM özellikleri ve açık onaylı fotoğraf paylaşım düzeni değişmedi.

## Kontrol
- Android makroları ile C++ sözdizimi kontrolü: PASS (Linux clang++ + JNI ve android/log.h test stubları)
- photo_transfer_protocol_test: PASS
- photo_browse_protocol_test: PASS
- developer_lock_test: PASS
- Gerçek Android ARM64 build / iki cihazda Photon testi: YAPILMADI.

## Test akışı
1. Her iki cihazda güncellenmiş libmultiplayermod.so kur.
2. Aynı Photon odasına gir; geliştirici şifresi ile GMM Connect'i aç.
3. Hedef oyuncuyu seç ve fotoğraf listesi iste.
4. `adb logcat -d -s multiplayermod:I multiplayermod:E | Select-String "GMM photo"` komutuyla TX/RX kaydını kontrol et.
5. Başarı göstergesi: `GMM photo TX: protocol=2 kind=R target=... ok=1`; alıcıda `GMM photo RX: protocol=2 kind=R sender=...`.
