# ImGui Unity Klavyesi ve Oyun Panelleri

Unity IL2CPP tabanlı Android oyununa eklenen C++ mod projesi. Çıktı,
**arm64-v8a** mimarisi için tek bir **libmultiplayermod.so** dosyasıdır.
Klavye ve paneller ImGui ile OpenGL ES 3 üzerinden çizilir; oyun bağlantıları
çalışma anında IL2CPP API ve metadata üzerinden çözülür.

## Özellikler

| Bölüm | İşlev |
| --- | --- |
| Klavye | Türkçe/UTF-8 metin girişi, harf ve sembol sayfaları, imleç hareketi, basılı tutarak silme |
| Metin araçları | Boyut etiketi, on renk seçeneği, oyunun font/sprite desteğine göre süzülen emoji adayları |
| Kare kısayolu | Metni `<size=7000>█` ile değiştirip uygun sohbet alanında gönderme |
| Metin kuralları | Oyun özelindeki iki etiket filtresi ve doğrulanabilen isim uzunluğu kontrolüyle entegrasyon |
| Kıyafet | Kozmetik katalog eşitlemesi, oyunun gardırop kart listelerinden kıyafet/şapka seçimi ve mevcut oda yayın akışı |
| Dans | Bekleme alanındaki yerel oyuncu modelini bulup mevcut emote/animator akışıyla seçim yapma |
| AI sohbet | Gemini API anahtarıyla seçilen oyuncunun yeni mesajlarına otomatik yanıt, kişilik ve sohbet hafızası |
| Oda bağlantısı | Dolu odaya bağlantı denemesini kullanıcı başlattığında tekrar etme |
| Reklam | Mevcut reklam engelleme entegrasyonu |
| Oyun paneli | Eşya spawn, düşman durdurma, düşman öldürme ve God Mode başlıkları; kontroller henüz devre dışı |

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

Uygun metin alanına dokununca özel klavye açılır. **Boyut** ile sayısal etiket,
**Renk** ile RGBA etiketi eklenir; **Emoji** desteklenen adayları açar.
`<`, `>` ve `█` kısayolları araç çubuğundadır. Entegrasyon hazır değilse oyunun
kendi klavyesi kullanılmaya devam eder.

### Kıyafet ve dans

Menüden **Kıyafet** bölümünü aç. Liste boşsa oyun içi gardırop ekranını açıp
tekrar kontrol et; panel oyunun gardırop kart listelerini okur. Seçimler mevcut
oyun/oda akışından uygulanır. **Dans** paneli geçerli yerel oyuncu ve animasyon
hedefini bekler; hedef hazır değilse işlem devre dışı kalır.

### AI sohbet

1. Odaya girip menünün **AI** bölümünü aç.
2. **API anahtarı gir** ile Gemini anahtarını yaz.
3. Yanıtlanacak oyuncuyu seç; istersen **Kişilik düzenle** ile talimat ekle.
4. Botu aç. Yalnız açıldıktan sonraki yeni mesajlara yanıt verir.

Anahtar yalnız bellekte tutulur, oyunu kapatınca silinir. İstekler arka planda
Android `HttpURLConnection` ile gönderilir; APK'da `INTERNET` izni gerekir.
Seçilen oyuncunun mesajları ve sohbet bağlamı Gemini'ye gönderilir. Yanıtlar
etiket karakterlerinden temizlenir ve 180 karakterle sınırlanır. Gönderim oyunun
bekleme süresine uyar; kullanıcının yazdığı taslağı ezmez.

## Kaynak düzeni

| Konum | İçerik |
| --- | --- |
| `.github/workflows/android.yml` | ARM64 derleme ve artifact iş akışı |
| `CMakeLists.txt`, `exports.map` | Kütüphane hedefleri, ABI denetimi ve sembol sınırı |
| `include/keyboard_api.h` | Dışarı açılan C API |
| `src/keyboard_api.cpp`, `src/keyboard_ui.cpp` | Grafik/giriş köprüsü ve arayüz |
| `src/unity_keyboard.cpp`, `src/il2cpp_runtime.*` | Unity metin alanı ve IL2CPP bağlantısı |
| `src/game_text_rules.cpp`, `src/username_gate.h`, `src/observer_target.cpp`, `src/hook_veneer.h` | Metin kuralları ve AArch64 native kontrol yardımcıları |
| `src/cosmetic_*`, `src/wardrobe_bridge.h`, `src/lobby_emotes.*` | Kozmetik, kıyafet ve dans |
| `src/chat_bot*`, `src/ad_block.*`, `src/auto_join*` | AI, reklam ve oda bağlantısı |
| `tests/` | Host testleri, sahte oyun API'leri ve ARM64 native örnekleri |
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

Testler metin tamponunu, klavye arayüzünü, başlangıcı, IL2CPP metin kurallarını,
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
