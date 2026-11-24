# GMM Connect — geliştirici erişimi ve Photon TX tanısı

## Erişim akışı

- Genel AI ekranında `Geliştirici` düğmesi kalır; bağımsız `GMM Connect` düğmesi kaldırıldı.
- `Geliştirici` sayfasında sabit şifreyle oturum açıldıktan sonra `GMM Connect'e git` görülür. Geliştirici oturumu kilitlenince alıcı ekranı da kapanır.
- Fotoğraf listesi isteme ve indirme, **C++ ağ işleyicisinde de** yerel geliştirici oturumunun açık olmasını gerektirir. Kilit kapanırsa devam eden indirme/listeme oturumu iptal edilir.
- Fotoğrafı **veren** kullanıcı geliştirici şifresini girmek zorunda değildir. Paylaşım isteği gelince `Kabul et / Reddet` penceresinden tek seferlik onay verir. Onay sadece `Pictures/GMMShare` dizinindeki uygun JPG/PNG adlarını açar; onaysız dosya taraması bulunmaz.
- Şifre hâlâ kullanıcıya verilmiş yerel menü korumasıdır; APK içindeki sabit şifre/hash gerçek bir sunucu kimlik doğrulaması veya tersine mühendisliğe karşı kırılmaz koruma değildir.

## Photon'da gözlenen hata

Kullanıcıdan gelen logda `hook=1`, `room=1`, `local_actor=2`, `peers=1`, `selected=3` fakat `TX kind=R ok=0`, `SendEvent failed: invoke=0 response=0` vardı. Bu, oyuncu seçme aşamasının çalıştığı fakat `PhotonNetwork.RaiseEvent` yönetilen çağrısının hata verdiği anlamına gelir. Olay alıcıya henüz ulaşmadığı için liste isteği penceresi açılamaz.

## Bu yamada yapılanlar

- Yönetilen çağrı başarısızsa **istisna türü** (örn. `System.InvalidOperationException`) loglanır; istisna içeriği/mesajı ve fotoğraf adları loglanmaz.
- `PhotonNetwork.RaiseEvent` olay kuyruğuna alınamadığında ve oyun sürümünde mevcutsa alternatif `LoadBalancingClient.OpRaiseEvent` denenir. Her ikisinin dört parametreli imzası Photon SDK ile aynıdır.
- Başarıdan sonra ikinci bir gönderme yapılmaz. `SendOptions.Encrypt` ve güvenilir gönderim tercihleri korunur; uçtan uca şifreleme iddiası yoktur.

## İki cihaz testi

Aynı mod sürümünü her iki Android cihazına yerleştirip aynı Photon odasına girin. Alan kullanıcı `Genel AI > Geliştirici > Şifreyi gir > GMM Connect'e git` yoluyla hedefi seçip listesi istesin. Veren kullanıcı kendi cihazında gösterilen paylaşım penceresinden onaylasın.

Windows PowerShell'de (her iki cihaz için ayrı):

```powershell
adb logcat -c
# Oyuna girin, hedef seçin, liste isteyin ve verici onayı verin.
adb logcat -d -v time -s multiplayermod:I multiplayermod:E | Select-String "GMM photo"
```

Beklenen: `TX kind=R ok=1` göndericide; `RX kind=R` fotoğrafı verecek cihazda; kabulden sonra `TX kind=G` ve `RX kind=L` alan cihazda. Eğer başarısızsa yeni `GMM photo TX exception via=... class=...` ve/veya `GMM photo TX Realtime client not available` satırlarını paylaşın.

**Sınır:** Gerçek iki Android cihazında Photon TX ve fotoğraf alma henüz doğrulanmadı. DEX değişmedi; bu güncelleme C++ kaynakları içindir.
