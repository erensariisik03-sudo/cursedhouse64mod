# ARM64 Android klavyesi ve oyun panelleri

Unity IL2CPP Android modu. Çıktı `libmultiplayermod.so`; yalnız **arm64-v8a**
hedeflenir. Entegrasyon çalışma anında IL2CPP metadata üzerinden çözülür.

## Bu güncelleme

- **AI → Sesli AI:** Ses → Ses, Metin → Ses, Ses → Metin ve Ses+Metin → Ses.
  Mevcut Gemini anahtarı, kişilik ve kişi seçimi kullanılır. Ses parçası önce
  metne çevrilir, AI cevabı oluşturulur, gerekiyorsa seslendirilir.
- AI paneli Kişiler, Kişilik, Yanıt ayarları, Sesli AI ve Geçmiş sayfalarına
  ayrıldı; uzun dikey ana panel sadeleştirildi.
- Granny/Grandpa/MomSpider klonlarında ilk 12 bağlama denemesi başarısız olsa
  bile 10 saniye arayla yeniden denenir. Geç oluşan sahne bağlantıları artık
  kalıcı olarak vazgeçilmesine yol açmaz; mevcut native AI akışı korunur.

- **Asıl düşmanları kaldırdıktan sonra klonlama düzeltildi.** Görme kurulumu artık
  yaşayan bir asıl düşman gerektirmez. Klonun kendi Eye, kamera, Animator ve
  PhotonView bağlantıları kullanılır; eksik DaysManager tekil sahne bileşeninden
  çözülür. Ortak hedef listeleri paylaşılmaz.
- Granny, Grandpa ve MomSpider'ın native görmesi/takibi/saldırısı korunur. Eksik
  görme bağlantısı varsa tanı kaydı artık hangi şartın eksik olduğunu gösterir.
- Ses, kısa ömürlü nesneleri yalnız arayarak izlenmez: normal native ses callback'i
  ve gerçek oyuncu adım olayları yakalanır. Adımlar 8 m, nesne sesleri 20 m
  içinde araştırılır. Oyuncuyu görme/saldırı, ses araştırmasından önceliklidir.
- Sağlıklı ve kontrolümüz altındaki klon 12 saniye hareket etmezse native devriye
  toparlaması denenir. Takılı saldırı durumu için 15 saniyelik kontrol vardır.
  Durdur ve gerçek silahla vurulma/ölüm durumları korunur; ışınlama uygulanmaz.
- **Oyun → Karakter hızı:** yürüyüş ve çömelme için 1x–5x çarpan, 2x/3x hazır
  düğmeler ve normal hıza dönüş. Kendi karakterine uygulanır, God Mode'dan
  bağımsızdır; çarpan üst üste birikmez, oyunun yeni temel hızını dikkate alır.
- Kapı desteği, God AI görünmezliği/düşme/benzin koruması, Infinity Ammo,
  Hamarat, kategori Durdur/Devam et ve host/client şaka oylaması korunur.
  Yeniden doğma süresi ve ayrı Ayarlar bölümü kaldırılmıştır.

## Kullanım

| Bölüm | İşlev |
| --- | --- |
| Android klavyesi | Yerel klavye, Unicode seçim/yapıştırma, 1024 karakter sınırı |
| Metin | Boyut, renk, desteklenen emoji ve kare kısayolu |
| AI sohbet | Gemini anahtarı, Kişiler, Kişilik, Yanıt ayarları ve Geçmiş |
| AI → Sesli AI | Ses/metin girişinden sesli veya yazılı yanıt; oda/yerel ses seçimi |
| Kıyafet | Oyun gardırobundan seçim ve aynı odaya çık-gir yayını |
| Oyun → Eşya spawn | Ele/yere oluşturma; doğrulanmış ağ prefabları |
| Oyun → Düşman öldür | Öldür, Kaldır ve Klonla sekmeleri |
| Oyun → God Mode | Ölüm/benzin/düşme koruması, AI görünmezliği, Crossbow/Shotgun Infinity Ammo ve Hamarat |
| Oyun → Karakter hızı | Kendi oyuncunun yürüme/çömelme hızı, 1x–5x ve normale dönme |
| Oyun → Hayalet modu | Joystick ile bakış yönünde uçuş; başlangıca dönüş |
| Oyun → Oyuncu atma | Host/client sohbet şakası; /evet–/hayır oylaması |
| Oyun → Zorluk | Gerçek hostun canlı AI zorluğunu değiştirmesi |

God Mode'u **takip/saldırı başlamadan önce** aç. Görünmezlik yalnız AI hedef
işaretini etkiler; diğer oyuncular seni görür. Kapanınca önceki normal hedef
etkinliği geri yüklenir; oyun arada yeni bir gizlenme isteği verdiyse korunur.
Oda/sahne/sahiplik değişiminde eski karaktere işlem yapılmaz.

Yerel görünmezlik durumu okunur; native RPC çağrısının başarılı dönmesi karşı
cihazda uygulandığının kanıtı değildir. Ağ yayını 3 saniyede bir yenilenerek geç
katılan cihazlar ve host değişimleri kapsanır. Gerçek cihaz doğrulaması gerekir.
Gizlenen hedefi bırakma kurtarması bu modun çalıştığı gerçek hostta uygulanır.
Modsuz uzak hostta önceden takılmış saldırıyı zorla sıfırlama garantisi yoktur.

Klonların tam native takip/saldırı AI'sini gerçek oda sahibi yürütür. Host olmayan
cihazın sahipli klon devriyesi tam host AI'sinin eşdeğeri değildir. Durdurma
host dışındaki cihazda yerel uygulanır; diğer cihazlardaki sonuç ağ yetkisine
bağlıdır. Kaldır, normal ölüm/yeniden doğma akışı değildir.

## Sesli AI kullanımı ve sınırları

1. AI bölümünden Gemini API anahtarını gir; Kişiler'den oyuncu seç veya
   Herkes'i aç. Herkes + isim şartı açıksa oyuncunun botun oyun adını söylemesi
   gerekir. Yazılı bot ana düğmesinin açık olması şart değildir.
2. Sesli AI sayfasında modu seç, sonra Sesli AI'yi aç. Oda değişince tekrar
   açmak gerekir. Kendi mikrofonumuz işlenmez; seçili uzak oyuncuların oyundan
   alınan sesi ve/veya onların yeni sohbet mesajları kullanılır.
3. Ses: oda / yerel seçeneğinde oda sesli sohbeti açık ve Photon Voice bağlantısı
   gerçekten odadaysa AI sesi Recorder'ın AudioClip kaynağından yayınlanır.
   Sesli sohbeti kapalı odada **Metin → Ses yalnız bu cihazda okunur**. Diğer
   modsuz oyuncuların sesi duyması, onların aynı Voice odasına bağlı olmasını
   gerektirir; yerel değişiklik tek başına bunu sağlayamaz. Ses: yalnız bu cihaz
   seçeneği açık odada da yerel çalmayı seçer.
4. AI konuşurken birincil mikrofon kaydı durdurulur; ek Recorder mikrofonları ve
   yeniden başlatma istekleri engellenir. Yayına mikrofon yerine yalnız AI klibi
   verilir. Bitince/durdurunca aynı oda ve geçerli bileşen için son mikrofon
   tercihi geri yüklenir. Oda değişiminde eski mikrofon yeniden açılmaz.
5. Konuşmayı durdur, çalan sesi ve bekleyen işleri iptal eder. Devam eden HTTP
   isteği zaman aşımına kadar tamamlanabilir; eski cevap uygulanmaz. Yeni
   konuşmalar için özelliği tamamen kapatmak üzere üstteki düğmeyi kullan.

Seçili kişilerin sesi Gemini'ye gönderilir; özelliği odadakilerin bilgisiyle
kullan. Ham ses dosyası, anahtar ve konuşma içeriği ADB tanı kaydına yazılmaz;
oyunun kendi logcat kayıtları ayrıca içerik içerebilir. Sesli AI geçmişi yalnız
oturum belleğindedir. İnternet, geçerli anahtar ve ses modellerine erişim/kota
şarttır; ücretsiz kota veya her hesapta model erişimi garanti edilmez.
Seslendirme artık Gemini TTS değil, cihazın kendi Android TextToSpeech motoruyla (Termux:API'nin kullandığı
motor) yapılır: ağ ve kota yoktur, Türkçe (tr-TR) ve düşük perde (erkek tonu) kullanılır. Cihazda
Türkçe ses paketi yüklü olmalıdır (Ayarlar → Dil → Metin okuma). Logcat'te `GMM_DIAG tts language-tr=`
sonucu 0 veya üzeri değilse motor/Türkçe yoktur ya da oyun TTS servisini göremiyordur.
Seslendirme/oynatma başarısızsa cevap normal sohbet akışına yazılır.

Bu sürüm anlık çift yönlü Live API kullanmaz. 0,7 saniye susma ile söz bölümü
sonlandırılır; bölüm başına en çok 12 saniye, en çok 8 uzak konuşmacı ve 4
bekleyen istek vardır. Tek arka plan işi çalışır; gecikme ağ ve modele bağlıdır.
Türkçe ve sessizlik algılaması gerçek oda gürültüsünde ayrıca denenmelidir.
API biçimi, PCM dönüşümü, mikrofon koruması ve oda kontrolleri sahte HTTP/IL2CPP
ile sınandı; fiziksel Android ve gerçek Gemini anahtarıyla uçtan uca test
**yapılmadı**. Ayrıntılı araştırma: `tools/VoiceAiFindings.md`.

## ADB kayıtları

`tools/Collect-GmmDiagnostics.ps1` dosyasını platform-tools yanında çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Collect-GmmDiagnostics.ps1
# Birden fazla cihaz varsa:
# .\Collect-GmmDiagnostics.ps1 -Serial CIHAZ_SERI_NO -Seconds 180
```

Oyunu aç, yeni bir odada God Mode açık/kapalı görünmezlik, benzin ve silahları
kontrol et. Görünmezken düşmanların başka görünür oyuncuya yönelmesini dene.
Önce asıl düşmanları kaldırıp Granny/Grandpa/MomSpider klonlarını oluştur.
**Görme testini God Mode kapalıyken yap**; God açıkken kendi karakterinin AI
hedefi gizlenir. Yürüme/adım ve yere eşya atma seslerini ayrı ayrı dene.
Karakter hızı menüsünde 1x/2x/3x ve normal hıza dönüşü kontrol et.
Sesli AI'da üç modu; açık odada uzak cihazın duyduğunu, kapalı odada yerel
okumayı; konuşurken mikrofonun kapanmasını, Durdur ve oda değişimini dene.
God açık/kapalı yüksekten düşmeyi dene. Klonları kilitsiz bir kapıya yaklaştır;
kilitli kapı ve aradaki duvar kontrolünü de dene. Görünür bir oyuncuyla takip,
köşe arkasına geçince devriyeye dönüş ve yere eşya atınca ses araştırmasını
kontrol et. Host/client şaka oylamasını ayrı ayrı dene; kimse çıkarılmamalı.
Granny/Grandpa/MomSpider kategorilerini ayrı ayrı durdur/devam ettir.
Bittiğinde PowerShell'de ENTER'a bas; Belgeler'e zaman damgalı kayıt ZIP'i gelir.
`-AdbPath`, `-OutputDirectory` ve `-PackageName` seçenekleri de vardır.

ZIP'te tam logcat, süzülmüş `diagnostics.txt`, crash tamponu, çıkış bilgisi ve
önce/sonra bellek raporları bulunur. `GMM_DIAG sight` yerel gizlenme ve native
RPC çağrılarını, `GMM_DIAG ai release-hidden` hedef kurtarmalarını kaydeder.
`GMM_DIAG clone` bağlama, eksik görüş şartı, kapı, ses, toparlama ve görüş kaybını; `fall_ready` /
`fall_blocked` düşme korumasını; `GMM_DIAG voice` ses hook/sonuç/oynatma durumunu; `GMM_DIAG player-speed` hız isteğini; `GMM_DIAG prank complete` şaka sonucunu gösterir.
Root gerekmez; log tamponları temizlenmez. Paylaşmadan önce tam logcat'i
kişisel içerik açısından kontrol et.

## Derleme

Android NDK r27c (`27.2.12479018`), CMake 3.22.1+ ve Ninja:

```bash
cmake -S . -B build-arm64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
  -DANDROID_STL=c++_static -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON
cmake --build build-arm64 --parallel
```

ImGui ve Dobby kaynakları ZIP'tedir. GitHub Actions iş akışı ARM64 derler ve
`libmultiplayermod.so` çıktısını verir. NDK dışındaki bağımlılıklar için indirme
gerekmez. Çıktı 16 KB LOAD hizalaması ve statik C++ çalışma zamanı kullanır.

## Testler

```bash
ASAN_OPTIONS=detect_leaks=0 bash tests/run.sh
python3 tests/diagnostics_test.py --pwsh /path/to/pwsh
```

Paket testleri yerel C++/ImGui ve sahte IL2CPP ile çalışır; görünmezlik,
sahiplik/sahne/oda kontrolleri, normal görünürlük isteğini koruma, RPC hız
sınırı/hata toparlama, üç düşman türünde hedef kurtarma, kategori durdurma ve
silah/benzin/düşme regresyonları, kapı engeli/kilit/bekleme kontrolleri, ses araştırması ve client şaka oylaması kapsanır. PowerShell testleri sahte ADB kullanır.
Bunlar gerçek Android/host cihazındaki sonucu doğrulamaz. Ayrıntılı derleme
kimliği, sonuçlar ve sınırlar `BUILD-INFO.txt` içindedir.
