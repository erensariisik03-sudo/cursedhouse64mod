# Android Klavyesi, ImGui Kısayolları ve Oyun Panelleri

Unity IL2CPP tabanlı Android oyununa eklenen C++ mod projesi. Çıktı,
**arm64-v8a** mimarisi için tek bir **libmultiplayermod.so** dosyasıdır.
Oyun içi metin girişini cihazın kendi Android klavyesi yapar. Kısayollar ve
paneller ImGui ile OpenGL ES 3 üzerinden çizilir; oyun bağlantıları
çalışma anında IL2CPP API ve metadata üzerinden çözülür.

## Özellikler

| Bölüm | İşlev |
| --- | --- |
| Android klavyesi | Cihazın mevcut klavyesiyle yazma, otomatik düzeltme, seçim ve yapıştırma; 1024 Unicode karakter sınırı |
| Metin araçları | Boyut etiketi, on renk seçeneği, oyunun font/sprite desteğine göre süzülen emoji adayları |
| Kare kısayolu | Metni `<size=7000>█` ile değiştirip uygun sohbet alanında gönderme |
| Metin kuralları | Oyun özelindeki iki etiket filtresi ve doğrulanabilen isim uzunluğu kontrolüyle entegrasyon |
| Kıyafet | Kozmetik katalog eşitlemesi, oyunun gardırop kart listelerinden kıyafet/şapka seçimi ve mevcut oda yayın akışı |
| Dans | Bekleme alanındaki yerel oyuncu modelini bulup mevcut emote/animator akışıyla seçim yapma |
| AI sohbet | Gemini API anahtarıyla seçilen oyuncunun yeni mesajlarına otomatik yanıt, kişilik ve sohbet hafızası |
| Oda bağlantısı | Dolu odaya bağlantı denemesini kullanıcı başlattığında tekrar etme |
| Reklam | Mevcut reklam engelleme entegrasyonu |
| Oyun paneli | Mevcut eşya prefablarından oluşturma, düşman durdurma/hız/animasyon/boyut, düşman nesnesi kaldırma ve kendi karakterine God Mode |

Klavye sınırı 1024 Unicode karakteridir. Oyun veya sunucu tarafındaki diğer
sınırların aynı uzunluğu kabul ettiği varsayılmaz. Emoji listesi bütün emojileri
kapsamaz; yalnız oyun tarafından desteklendiği saptanan adaylar gösterilir.

## GitHub Actions ile derleme

1. Bu klasörün içeriğini GitHub deposunun köküne yükle. `CMakeLists.txt`, `src/`,
   `third_party/` ve `.github/workflows/android.yml` aynı kökte bulunmalı.
2. **Actions → Build Android .so (ARM64)** iş akışını aç. Push ve pull request
   ile otomatik, **Run workflow** ile elle çalıştırılabilir.
3. İş akışı host testlerini çalıştırır, Android kütüphanesini derler ve strip eder.
4. Tamamlandığında **multiplayermod-arm64-v8a** artifact'ini indir.
   Kütüphane artifact içinde `libmultiplayermod.so` adıyla bulunur.

Artifact saklama süresi üç gündür. Bu depo APK üretmez; `.so` dosyasının oyuna
nasıl yükleneceği mevcut yükleyici/entegrasyon yöntemine bağlıdır.

### Derleme ayarları

| Ayar | Değer |
| --- | --- |
| Hedef ABI | `arm64-v8a` |
| Android platformu | API 21 ve üzeri |
| NDK | `27.2.12479018` |
| CMake | `3.22.1` ve üzeri |
| Dil | C++17 |
| Grafik | OpenGL ES 3 |
| C++ çalışma zamanı | `c++_static` |
| ELF sayfa hizalaması | 16 KiB |
| Dışa açılan semboller | `mm_*`, `JNI_OnLoad` |

CMake yalnız `ANDROID_ABI=arm64-v8a` kabul eder. ImGui ve Dobby kaynakları
`third_party/` içinde bulunduğundan derleme sırasında bu bağımlılıklar indirilmez.
Dobby'nin bu paket içindeki kaynakları hedef mimari için uyarlanmıştır.

### İsteğe bağlı yerel Android derlemesi

NDK, CMake ve Ninja kurulu bir Bash ortamında:

```bash
cmake -S . -B build/android -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21 \
  -DANDROID_STL=c++_static \
  -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON
cmake --build build/android --parallel
```

Çıktı: `build/android/libmultiplayermod.so`.

## Kullanım

### Klavye

Oyun içindeki uygun metin alanına dokununca **Android'in kendi klavyesi** açılır.
ImGui harf klavyesinin yerine **Boyut, Renk, `<`, `>` ve `█`** kısayolları
ile **Yapıştır** düğmesi bulunan küçük bir çubuk görünür.

- Android klavyesi normal yazma, otomatik düzeltme, metin seçimi ve gönderme
  işlemlerini yürütür. Kısayol eklemeleri güncel imleç/seçime uygulanır.
- **Boyut** sayısal seçiciyi, **Renk** renk listesini açar. Emojiler doğrudan
  Android klavyesinden girilir; çubukta ayrı Emoji düğmesi yoktur.
  Yarım bırakılan boyut/renk taslakları oyun alanına gönderilmez.
- **Küçült** yalnız kısayol çubuğunu küçültür; Android klavyesini kapatmaz.
  **Kısayollar** düğmesiyle tekrar açılır.
- Klavye alanı Unity tarafından bildiriliyorsa çubuk klavyenin üstüne yerleşir.
  Alan bilgisi boş veya kullanılamazsa ekranın üstünde açılır. Cihazdaki klavye
  yüksekliği ve pencere davranışı ayrıca test edilmelidir.
- Şifre alanlarında kısayollar devre dışıdır; parola çubukta gösterilmez.

1024 sınırı yalnız ImGui tamponuna bağlı değildir: aktif oyun alanındaki Android
metni de Unicode karakteri bazında sınırlandırılır. Sınır uygulaması değişiklik ve
gönderme bildirimlerinden önce yapılır; emoji vekil çiftleri bölünmez. Eski TMP
UTF-16 sınırı oturum sırasında gevşetilir, alan odaktan çıkınca özgün ayarı geri
yüklenir. Mevcut iki etiket filtresini kapatan `game_text_rules` entegrasyonu ve
isim uzunluğu kontrolü korunur; sunucu kuralları ve diğer doğrulamalar korunur.

AI panelinin API anahtarı/kişilik girişleri oyun içi TMP alanı değildir;
bu iki panel alanında mevcut bağımsız ImGui editörü kullanılmaya devam eder.

### Kıyafet ve dans

Menüden **Kıyafet** bölümünü aç. Liste boşsa oyun içi gardırop ekranını açıp
tekrar kontrol et; panel oyunun gardırop kart listelerini okur. Seçimler mevcut
oyun/oda akışından uygulanır. **Dans** paneli geçerli yerel oyuncu ve animasyon
hedefini bekler; hedef hazır değilse işlem devre dışı kalır.

### Oyun kontrolleri

Bu sürüm ağ inceleme eklenmeden önceki `system-terminal-testfix` tabanından
devam eder. Kıyafet inceleme düğmesi dahil değildir. Mevcut klavye/AI akışı korunur.

- **Eşya spawn:** Kendi `HandlePickUpItemInteractions.handItemTies` listesindeki
  gerçek prefabları listeler. Seçilen eşyayı **önüme oluştur** düğmesi, oyunun
  eşya bırakırken kullandığı kaynak dizini ve `PhotonNetwork.Instantiate` yoluyla
  önündeki bırakma noktasında oluşturur. Eşyayı ardından normal şekilde alırsın.
  Kaynak adı tahmin edilmez; boş/çift kimlikli prefablar listelenmez. İki deneme
  arasında bir saniye vardır. Oyunun kaynak havuzu/oda kuralları çağrıyı reddedebilir.
- **Düşman durdur:** Granny, Grandpa, MomSpider, Spider, Crow ve Rat için seçim
  sunar. Durdur/devam, hız çarpanı, animasyon çarpanı, boyut ve sıfırlama vardır.
  Bu kontroller oda yöneticisinde açılır. Crow için doğrulanan ayrı bir hareket
  hızı alanı olmadığından o sürgü kapalıdır; animasyon ayarı kullanılabilir.
  Crow/Rat değişiklikleri yereldir. Özgün değerler saklanır; sıfırlama bunları
  geri yükler, sonradan oyun tarafından değiştirilmiş alanlar korunur.
- **Düşman öldür:** Bu başlık altındaki işlem **nesne kaldırmadır**; normal
  vurulma, süreli bayılma, ragdoll, başarı veya ödül akışını taklit etmez.
  Granny/Grandpa/MomSpider/Spider için yalnız sahibi olduğun Photon nesnesine
  oyunun normal ağdan silme API'si çağrılır. Crow/Rat için yerel Unity nesnesi
  kaldırılır. Sahne nesnesi/PUN kuralları kaldırmayı reddedebilir; durum metni
  çağrıyı bildirir, diğer istemcide sonuç alındığını iddia etmez.
- **God Mode:** Yalnız kendi oyuncu nesnenin doğrulanan AI/ölüm girişlerini ve
  playerManager ölüm girişini engeller. MomSpider ölüm coroutine'i kendi oyuncuna
  aitse durdurulur. Kapalıyken, başka oyuncuda, farklı odada/sahnede veya Unity
  iş parçacığı dışında özgün metotlar çalışır. Ölmüş karakteri diriltmez; ölüm
  animasyonu başlamadan açılmalıdır. Bütün çevresel tehlikeleri ve sunucudaki
  kararları engellediği doğrulanmış değildir.

Metot/alanlar çalışma anında metadata isimleriyle çözülür; gönderilen eski C++
dosyasındaki sabit RVA'lar ve Grandpa'ya kopyalanmış Granny offset'leri kullanılmaz.
Yeni bağlantıların inceleme envanteri `tools/gameplay_reference.csv` içindedir;
bu dosya çalışma anı adres yapılandırması değildir. Aynı ariteye sahip
`PhotonNetwork.Destroy` overload'ları parametre tipiyle ayrılır.

Renderer yalnız sayısal ayarları ve oturum kimlikli komutları kuyruğa koyar.
Unity çağrıları Unity karesinde yapılır; eski oda/karakterden gelen ve üç saniyeden
eski komutlar atılır. Yeni oda/sahne/karakter oturumunda ayarlar sıfırlanır.
Desteklenmeyen metadata veya henüz oluşmamış karakterde kontroller kapalı kalır.
God Mode bağlantılarından biri eksikse bu düğme etkinleştirilmez.

Yeni host testleri kapalı God Mode'un özgün çağrıları iletmesini; oyuncu, oda,
sahne ve iş parçacığı sınırlarını; coroutine ve GC handle davranışını; hızın
üst üste çarpılmamasını ve geri yüklemeyi; eski UI komutlarının reddini denetler.
Yerel build/C++ test derlemesi çalıştırılmadı; GitHub CI ve cihaz davranışı
ayrıca doğrulanmalıdır.

### AI sohbet

1. Odaya girip menünün **AI** bölümünü aç.
2. **API anahtarı gir** ile Gemini anahtarını yaz.
3. Yanıtlanacak oyuncuyu seç; istersen **Kişilik düzenle** ile talimat ekle.
4. Botu aç. Yalnız açıldıktan sonraki yeni mesajlara yanıt verir.

Mevcut ayar sistemi API anahtarını ve bot tercihlerini uygulamanın özel
`files/gmm_ai.cfg` dosyasında saklar; anahtar loglanmaz. İstekler arka planda
Android `HttpURLConnection` ile gönderilir; APK'da `INTERNET` izni gerekir.
Seçilen oyuncunun mesajları ve sohbet bağlamı Gemini'ye gönderilir. Yanıtlar
etiket karakterlerinden temizlenir ve 180 karakterle sınırlanır. Gönderim oyunun
bekleme süresine uyar; kullanıcının yazdığı taslağı ezmez.

### Hazır yanıtlar ve isim hatırlatması

**Ada göre süz** açıkken oyun adının yanında cümlede ayrı sözcük olarak geçen
`zeka`, `yapay` ve `yapayzeka` da bota hitap sayılır. Büyük/küçük harf fark etmez;
`yapay zeka` yazımı da çalışır. `zekalı` gibi kelimelerin içinden eşleşme yapılmaz.


- `merhaba`, `mrb`, `slm`, `selam`, `sa`, `nasılsın`, `nasılsınız`, `naber` ve
  yaygın kısa selamlaşma birleşimleri AI'ye gönderilmeden yerel yanıt alır.
  Aynı oyuncuya bu tür yanıtlar en sık 30 saniyede bir verilir.
- Selamlaşmanın yanında gerçek bir soru varsa soru korunur: örneğin
  `Bot merhaba, oyun nasıl oynanır?` hazır selam yanıtına dönüştürülmez.
- Kahkaha/random süzgeci korunur. Model `SKIP`, `Ad: SKIP` veya `Ad, SKIP`
  döndürürse mesaj sessizce elenir; kontrol işareti sohbet satırına veya yanıt
  hafızasına eklenmez. Gönderim sınırında her parça tekrar kontrol edilir.
- Bot açık, **Ada göre süz** açık ve odada başka oyuncu varken ilk isim
  hatırlatması 15 saniye sonra, devamı iki dakikada bir hazırlanır:
  `Soru için adımı ("BotAdı") veya zeka, yapay, yapayzeka yazıp sorunuzu ekleyin.`
  Metin gerçek oyun adını kullanır; bu mesaj için AI isteği yapılmaz.
- Normal yanıtlar ve bekleyen sorular hatırlatmadan önce gelir. Hatırlatma da
  oyunun sohbet bekleme süresine, metin alanının odağına ve taslak korumasına
  uyar. Gönderilemeyen hatırlatma 20 saniyede bırakılır; mesajlar biriktirilmez.
- Bot/isim süzgeci kapatılınca, odadan çıkınca veya ad değişince eski hatırlatma
  iptal edilir. Eski bot oturumundan tamamlanan AI yanıtı da gönderilmez.

### Yerel küfür uyarısı

Bot açıkken açıkça bota yöneltilen belirli küfürler yerel kelime/ifade kurallarıyla
saptanır. Bu mesajlar AI'ye gönderilmez; **iki sabit, sert uyarı** normal yanıtların
önüne alınır. İlk uyarı uygun ilk sohbet anında, ikincisi oyunun bekleme süresi
izin verince gönderilir. Küfürle karşılık verilmez.

`lan`, `amk`, `aq`, sıradan argo ve hakaret kelimeleri bu sabit küfür kuralını
tetiklemez; mevcut isim/oyuncu süzgecinden geçiyorsa normal AI akışına bırakılır.
Başka oyuncuya yöneltilen sözler ve kelimenin anlamını soran/aktaran cümleler için
sabit uyarı verilmez. Bu yapı sınırlı bir yerel sözlüktür; her yazım hilesini ya da
bağlamı anladığı varsayılmaz.

Aynı oyuncuya yeni uyarı çifti en sık **60 saniyede bir**, genel olarak en sık
**10 saniyede bir** hazırlanır. Bekleyen uyarı çifti varken yeni çift eklenmez.
Taslak/odak koruması, sohbet bekleme süresi ve zaman aşımı bu mesajlarda da geçerlidir.

### Açılış/kapanış ve model durum akışı

Bot açılırken, kapatılırken, kişilik ve model değişiminde oyun sohbetine her
olay için farklı, **tamamen kurgusal Linux terminali** satırları hazırlanır.
Gerçek oyun sınıfları, API isimleri, sağlayıcı veya gerçek model kimliği gösterilmez.
Son satır herkesin anlayacağı biçimde `AI AKTİF`, `MODEL: İMAM`, `AI KAPANDI`
veya `AI MODELİ GÜNCELLENDİ` der. İlk satır mevcut büyük kare kısayolunu
`<#888F>` ile griye boyar. Açılış 24, kapanış/kişilik/model geçişleri 18 satırdır;
akış saniyede en çok 6 satır gönderir. Açılışta kurgusal bir konsol servisinin
hazırlanması, CMake/Ninja derleme çıktıları ve servis başlatma adımları görünür.
Komutlar yalnız dekoratif metindir; cihazda çalıştırılmaz.

Bu gösteri mesajları hazırlanırken oyunun isim okuyucusu yalnız eşzamanlı sohbet
gönderimi süresince `System` döndürür. Kalıcı oyuncu adı veya Photon oyuncu
özellikleri değiştirilmez; normal sohbet ve AI cevapları kendi adını kullanır.
Referans ARM64 gönderim akışında ad, UTF-8 sohbet yükü hazırlanırken okunur.
Bu bağlantı kurulamazsa terminal mesajları atlanır, normal adla gönderilmez.
`System` bir sohbet görüntü adıdır; ağdaki Photon gönderen/aktör kimliği değişmez
ve sunucu mesajın hangi oyuncudan geldiğini görebilir. Cihazdaki ve diğer
istemcideki görünüm derleme sonrasında ayrıca doğrulanmalıdır.

Yerel bekleme izni artık bütün `ChatManager.SendMessage` çağrılarına uygulanır:
normal gönder düğmesi, kare kısayolu ve AI gönderimleri aynı genel kancadan geçer.
Bayrak yalnız çağrı boyunca geçici açılır ve önceki kapalı değer geri konur;
kalıcı açık bayrak veya yeni ağ paketi kullanılmaz. AI'nin ayrı yanıt aralıkları ve
taslak/odak koruması korunur. Gerekli API veya kanca kullanılamazsa oyunun olağan
kontrolü geçerli kalır. Sunucu sınırları bu değişiklikten etkilenmez.

Oyuncu yazarken/taslak varken gönderim bekler. Akış beş saniyede tamamlanamazsa
kalanı iptal edilir; yeni olay eskisinin yerine geçer ve odadan çıkınca temizlenir.
Kapatma animasyonu, AI kapandıktan sonra da tamamlanabilir.

## Kaynak düzeni

| Konum | İçerik |
| --- | --- |
| `.github/workflows/android.yml` | ARM64 derleme ve artifact iş akışı |
| `CMakeLists.txt`, `exports.map` | Kütüphane hedefleri, ABI denetimi ve sembol sınırı |
| `include/keyboard_api.h` | Dışarı açılan C API |
| `src/keyboard_api.cpp`, `src/keyboard_ui.cpp` | Grafik/giriş köprüsü, Android kısayol çubuğu ve bağımsız panel editörü |
| `src/native_shortcuts.h` | Oturum kimlikli komut kuyruğu, kopyalanmış metin/seçim ve UTF-16/UTF-8 dönüşümü |
| `src/unity_keyboard.cpp`, `src/il2cpp_runtime.*` | Unity metin alanı ve IL2CPP bağlantısı |
| `src/game_text_rules.cpp`, `src/username_gate.h`, `src/observer_target.cpp`, `src/hook_veneer.h` | Metin kuralları ve AArch64 native kontrol yardımcıları |
| `src/cosmetic_*`, `src/wardrobe_bridge.h`, `src/lobby_emotes.*` | Kozmetik, kıyafet ve dans |
| `src/chat_bot*`, `src/ad_block.*`, `src/auto_join*` | AI, reklam ve oda bağlantısı |
| `tests/` | Host testleri, sahte oyun API'leri, botun HTTP/gönderim akışı ve ARM64 native örnekleri |
| `tools/` | Dump/metadata inceleme ve PowerShell log toplama araçları |
| `previews/` | Arayüz örnek görselleri |
| `third_party/` | ImGui, Dobby ve bunların lisansları |
| `CLAUDE.md` | Güncel geliştirme kuralları |

`tools/analyze_metadata.cpp` bağımsız bir analiz aracıdır; oyun kütüphanesinin
kaynak listesine dahil değildir. İnceleme girdileri ve yerel derleme çıktıları
kaynak paketine dahil edilmez.

## Test ve doğrulama

Linux/Bash ve `g++` ile:

```bash
bash tests/run.sh
```

Testler Android yazımıyla eşzamanlı kısayol eklemeyi, güncel seçim/imleci,
oturum değişimini, emoji sınırlarını, callback/gönderme öncesi sınırlandırmayı,
çubuğun beş ekran boyutundaki davranışını, metin tamponunu, başlangıcı, IL2CPP metin kurallarını,
ARM64 isim kontrolünü ve observer örneklerini, kozmetik/kıyafet, dans ve bot
mantığını kapsar. Testler host üzerinde çalışır; ARM64 komut örneklerinin
çözümlenmesi telefonda o komutların çalıştırıldığı anlamına gelmez.

Android derlemesi de cihazdaki davranışı doğrulamaz. Özellikle kıyafetin başka
istemcide görünmesi, bekleme alanı dansı, JNI ağ istekleri ve gerçek oyun
sürümündeki metadata eşleşmesi cihazda ayrıca kontrol edilmelidir.

Hata raporunda oyun sürümünü, cihazı, kapanmanın hangi adımda olduğunu ve varsa
logu paylaş. `tools/Collect-GmmCrash.ps1` ve `tools/Collect-GmmGarden.ps1`
Windows/ADB ile ilgili tanılama kayıtlarını toplamaya yardımcı olur.

## Bağımlılıklar ve lisanslar

ImGui lisansı `third_party/imgui/LICENSE.txt`, Dobby lisansı
`third_party/dobby/LICENSE` içindedir. İç bağımlılıkların lisans bildirimleri de
korunmuştur. ImGui OpenGL backend'indeki mevcut context kaybı uyarlaması korunur.

Teknik API başvuruları: [Unity TMP_InputField kaynak kodu](https://github.com/Unity-Technologies/uGUI/blob/main/com.unity.ugui/Runtime/TMP/TMP_InputField.cs),
[Unity TouchScreenKeyboard API kaynak kodu](https://github.com/Unity-Technologies/UnityCsReference/blob/master/Runtime/Export/TouchScreenKeyboard/TouchScreenKeyboard.bindings.cs).
