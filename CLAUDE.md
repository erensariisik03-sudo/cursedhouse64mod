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
- Klavye entegrasyonu hazır değilse oyunun kendi klavyesini açık bırakın.
  Aktif metin alanının ayarlarını oturum bitince geri yükleyin.
- Metin, parola, sohbet içeriği veya API anahtarını loglamayın. API anahtarı
  yalnız bellekte kalır ve HTTP başlığında gönderilir.
- Native isim kontrolünde özgün alt sınırı koruyun; belirsiz komut örüntülerini
  reddedin. Observer yalnız AArch64 doğrudan tail branch zincirini izler;
  komşu API girişleri üzerine yazmayı reddeder.
- Kıyafet/dans/sohbet için mevcut oyun akışını ve yerel sahiplik kontrollerini
  koruyun. Yeni ağ paketleri, tahmini RPC veya bulut/satın alma çağrıları eklemeyin.
- Oyun panelinin devre dışı kontrolleri yalnız arayüz taslağıdır.
- Dobby bu paket için uyarlanmıştır. Bağımlılık lisanslarını ve ImGui'nin
  mevcut context kaybı uzantısını koruyun.
- İlgili değişiklikleri tests/run.sh ile doğrulayın. Host testleri, Android
  derlemesi ve cihaz testinin sonuçlarını ayrı belirtin.
