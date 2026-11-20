# Proje geliştirme notları

Önce README.md dosyasını okuyun. Android çıktısı yalnız arm64-v8a için
libmultiplayermod.so dosyasıdır. Derleme GitHub Actions üzerinden yapılır.

- C++17, OpenGL ES 3, statik C++ çalışma zamanı ve 16 KiB ELF hizalamasını koruyun.
- Dışarı yalnız mm_* ve JNI_OnLoad sembolleri açılır.
- IL2CPP metotlarını ve alanlarını çalışma anında metadata/API üzerinden çözün;
  dump dosyalarından sabit RVA veya alan offset'i taşımayın.
- Yönetilen çağrıları Unity'nin betik iş parçacığında yapın. Başlangıç iş
  parçacığını VM'ye bağlamayın. Gerçek bir Unity karesi gözlenmeden başlatmayın.
- İş parçacıkları arasında kopyalanmış değerleri ve kilitli köprüleri kullanın;
  yönetilen nesne referanslarını GC handle ile koruyun.
- Oyun içi TMP alanlarında Android klavyesini açık bırakın. ImGui yalnız kısayol
  işlemleri gönderir; renderer metin kopyasıyla native yazıyı ezmeyin.
- 1024 Unicode karakter sınırını değişiklik/gönderme callbacklerinden önce koruyun.
  İmleç/seçimde UTF-16 ve UTF-8 dönüşümünü doğru yapın.
- Klavye entegrasyonu hazır değilse oyunun kendi klavyesini açık bırakın.
  Aktif metin alanının ayarlarını oturum bitince geri yükleyin.
- Metin, parola, sohbet içeriği veya API anahtarını loglamayın. Mevcut özel
  ayar dosyasını koruyun; ağ isteğinde anahtar HTTP başlığında gönderilir.
- Native isim kontrolünde özgün alt sınırı koruyun; belirsiz komut örüntülerini
  reddedin. Observer yalnız AArch64 doğrudan tail branch zincirini izler;
  komşu API girişleri üzerine yazmayı reddeder.
- Kıyafet/dans/sohbet için mevcut oyun akışını ve yerel sahiplik kontrollerini
  koruyun. Yeni ağ paketleri, tahmini RPC veya bulut/satın alma çağrıları eklemeyin.
- Oyun panelinin devre dışı kontrolleri yalnız arayüz taslağıdır.
- Dobby bu paket için uyarlanmıştır. Bağımlılık lisanslarını ve ImGui'nin
  mevcut context kaybı uzantısını koruyun.
- Kullanıcı açıkça istemedikçe yerel build veya C++ test derlemesi çalıştırmayın.
  Eko modda ilgili kaynakları kontrol edip ZIP sağlayın; derleme GitHub
  üzerinden kullanıcı tarafından yapılır. Yapılmayan doğrulamayı yapılmış göstermeyin.

- SKIP yalnız iç kontrol işaretidir; model yanıtını parçalamadan ve sohbete
  göndermeden önce eleyin. Hazır selamları AI'ye göndermeyin, gerçek soruyu
  selam eşleştirmesiyle yutmayın. İsim hatırlatmaları yalnız bot/isim süzgeci
  açıkken çalışır; normal yanıtlar, taslaklar ve oyun bekleme süresi önceliklidir.

- Kullanıcının istediği durum animasyonunda yalnız tek SendMessage çağrısı
  çevresinde canSendMessage için kapsamlı geri-yükleme koruması kullanılır.
  Normal sohbetin kontrolünü kaldırmayın; olay akışını 6 satır/sn ve 5 sn ile sınırlayın.
