
## SEDA ONNX modeli APK assets içinde (2026-11)

Bu sürüm artık modeli APK içinden otomatik yükler. **APK'nin kökünde** şu yollar bulunmalı:

```text
assets/gmm_seda/asset-version.txt
assets/gmm_seda/tokens.txt
assets/gmm_seda/c32/encoder.int8.onnx
assets/gmm_seda/c32/decoder.onnx
assets/gmm_seda/c32/joiner.int8.onnx
```

`assets/bin/Data/datapack.unity3d` dosyasını **değiştirmeyin**; `assets/gmm_seda/` ayrı bir klasördür.

Yeni `libmultiplayermod.so` (kaynak yeniden derlenmeli) Android NDK AssetManager ile
`assets/gmm_seda` içindeki dört gerekli dosyayı ilk ASR kullanımında APK'den
`Context.getFilesDir()/gmm_seda` klasörüne **otomatik** çıkarır. Sherpa-ONNX bir
`AAsset` tanıtıcısı değil gerçek dosya yolu istediğinden bu çıkarma gereklidir.
Model güncellemeleri `asset-version.txt` kimliğine göre yeniden çıkarılır; yazma
`*.tmp-XXXXXX` geçici dosyası ve atomik rename kullanır.

GitHub Actions çıktısında `dist/assets/gmm_seda/` hazır bulunur. Apktool veya APK
editöründe **dist içindeki `assets` klasörünün içeriğini APK'nin `assets/`
klasörüne** yerleştirin; `dist/assets` adlı ikinci bir klasör oluşturmayın.
Native `.so` kütüphanelerini `lib/arm64-v8a/` içine koyun. APK'yi yeniden
imzalayıp yükleyin. Dosyaları `Android/data` altına manuel kopyalamanız gerekmez.

APK editörü `asset-version.txt` dahil dört model dosyasını yerleştirmeli. Logcat'te
`GMM_SEDA_ASSETS ready (APK assets -> internal files/gmm_seda)` mesajı ilk
yüklemeyi doğrular. Manuel kurulum yedeği eskisi gibi çalışır.

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

Uzak oyuncuların sesi önce cihaz üzerinde SEDA v0.2-Tiny ile metne çevrilir.
Gemini'ye ham ses değil, yalnızca ortak yerel filtreden geçen transkript gönderilir.
AI panelindeki `Mesaj filtresi` ve `Adla çağırma` anahtarları artık aynı
`message_gate::Evaluate` katmanında hem yazılı mesajı hem de ses transkriptini
kontrol eder. Genel filtre kapatıldığında kahkaha/gürültü/yan konuşma elemesi
kapanır; isim filtresi bağımsız çalışır. Seçili oyuncu modunda seçilmiş kişi
zaten hedef kapsamıdır; Herkes modunda isim filtresi açıksa `zeka`, `yapay`,
`yapayzeka` veya karakter adıyla hitap aranır. Selamlaşmalar mevcut davranışa
uygun olarak yerel hazır cevap alır; Gemini çağrısı yapılmaz.
SEDA modeli Türkçe içindir; başka diller/özel oyun adları doğru tanınmayabilir.
Bu paket düşük gecikme için 0,64 saniyelik `c32` akışını ve greedy decoder'ı
seçer. Model kartındaki Türkçe FLEURS testinde bu hızlı ayar %17,30 WER
bildirir; daha doğru ayarlar daha büyük dil modeli/ek işlem gerektirir. Gürültülü
Photon/telefon sesi için aynı doğruluk garanti edilmez. Yaklaşık 7 MB değeri
akustik model dosyası boyutudur, toplam çalışma RAM'i değildir; cihazda ayrıca
ölçülmelidir. Özelliği odadakilerin bilgisiyle kullan. Ham ses dosyası, anahtar ve konuşma içeriği ADB tanı kaydına yazılmaz;
oyunun kendi logcat kayıtları ayrıca içerik içerebilir. Sesli AI geçmişi yalnız
oturum belleğindedir. İnternet, geçerli anahtar ve ses modellerine erişim/kota
şarttır; ücretsiz kota veya her hesapta model erişimi garanti edilmez.
Seslendirme artık Gemini TTS değil, cihazın kendi Android TextToSpeech motoruyla (Termux:API'nin kullandığı
motor) yapılır: ağ ve kota yoktur, Türkçe (tr-TR) ve düşük perde (erkek tonu) kullanılır. Cihazda
Türkçe ses paketi yüklü olmalıdır (Ayarlar → Dil → Metin okuma). Logcat'te `GMM_DIAG tts language-tr=`
sonucu 0 veya üzeri değilse motor/Türkçe yoktur ya da oyun TTS servisini göremiyordur.
Seslendirme/oynatma başarısızsa cevap normal sohbet akışına yazılır.

Bu sürüm anlık çift yönlü Live API kullanmaz. Ses bölümü en az 0,55 saniye konuşma
ve en az 0,35 saniye ses etkinliği içerdiğinde tanımaya alınır; 0,9 saniye
sessizlikte biter. Bölüm başına en çok 12 saniye, en çok 8 uzak konuşmacı ve 4
bekleyen istek vardır. SEDA v0.2-Tiny c32 greedy decoder tek CPU thread'inde
çalışır. Model dosyaları ayrı pakettir ve aşağıdaki kurulum adımı gerekir.
Gerçek oda gürültüsünde Türkçe tanıma ayrıca cihazda test edilmelidir.
Yerel ASR ve ortak filtre testleri sahte ASR/HTTP/IL2CPP ile doğrulanır; fiziksel
Android ve gerçek Gemini anahtarıyla uçtan uca test **yapılmadı**. Ayrıntılar:
`tools/VoiceAiFindings.md`.

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

Android NDK r27c (`27.2.12479018`), CMake 3.22.1+, Ninja, Git, curl ve zip
gerekir. SEDA açık olduğu için sherpa-onnx v2.0.3 Android ARM64 C API ile
ONNX Runtime bağımlılıkları da derlenir. GitHub Actions bunları otomatik indirir
ve derler; workflow `dist/` içinde mod kütüphanesi, `libsherpa-onnx-c-api.so`,
`libonnxruntime.so` ve ayrı `seda-v0.2-tiny-c32.zip` model paketini üretir.
Üç `.so` dosyası da aynı ARM64 yükleme paketinde birlikte tutulmalıdır.

Modeli oyunun uygulama paket adına göre yükle (örnek paket adını kendi oyun
paket kimliğinle değiştir):

```powershell
.\tools\install_seda_android.ps1 -PackageName com.example.game -ModelArchive .\seda-v0.2-tiny-c32.zip
```

Kurulum hedefi `/sdcard/Android/data/<paket>/files/gmm_seda` dizinidir. Android
cihazındaki ADB/scoped-storage politikası `adb push` işlemini engellerse model
dosyalarını aynı uygulama dizinine yetkili bir yöntemle kopyala. Oyun sürecini
yeniden başlat. Model dosyaları ana `.so` içine gömülmez.

GitHub Actions ARM64 NDK derlemesini, sherpa-onnx C API bağlantısını ve model
paketini üretir. Manuel CMake derlemesinde önce sherpa-onnx `v2.0.3`
`build-android-arm64-v8a.sh` betiğini `SHERPA_ONNX_ENABLE_C_API=ON`,
`SHERPA_ONNX_ENABLE_JNI=OFF`, `SHERPA_ONNX_ENABLE_TTS=OFF` ve
`SHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION=OFF` ile çalıştır; ardından
`-DMM_ENABLE_SEDA=ON -DSHERPA_ONNX_ROOT=<sherpa-install-prefix>` ver.
Model lisansı CC BY-SA 4.0'dır; model ZIP'inde `MODEL-CREDITS.txt` ile
atıf ve lisans bağlantısı bulunur. Yeniden dağıtırken lisans koşullarına uy.

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


## Fotoğraf aktarımı — hazırlık sürümü

Geliştirici menüsüne şifreli alıcı kilidi ve fotoğraf parçaları bütünlük test çekirdeği eklendi. **İki cihaz arasında gerçek aktarım henüz bağlanmadı.** Ayrıntılar için `FOTO_TRANSFER_PROTOTIP_NOTLARI.md`.
