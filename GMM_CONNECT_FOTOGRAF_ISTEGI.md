# GMM Connect — Fotoğraf istekli paylaşım (ilk iki cihaz denemesi)

Bu sürümde Geliştirici şifresi yalnızca geliştirici menüsü içindir. **Fotoğraf verecek cihazın geliştirici bölümüne girmesine gerek yoktur.** Normal mod menüsünün üstünde fotoğraf paylaşım isteği görünebilir; Kabul / Reddet butonları vardır.

## Akış

1. Her iki cihazda aynı güncel ARM64 `libmultiplayermod.so` derlemesini çalıştırın ve aynı Photon oyun odasına girin.
2. Fotoğrafı verecek cihaz, paylaşılmasına izin verdiği **256 KB veya daha küçük JPG/PNG** fotoğrafı `/sdcard/Pictures/GMMShare/` içine koysun. Bu bir *paylaşım klasörüdür*; oyun bütün galeriyi otomatik listelemez.
3. Fotoğrafı almak isteyen cihaz: `AI > GMM Connect` bölümünü açsın. Oyuncu listesinden **fotoğrafın sahibi olan oyuncuyu** seçsin. Bu listedeki satırlar düğmedir; `Hedef oyuncu seç` yalnızca başlıktır.
4. `Hedef oyuncudan fotoğrafları iste` düğmesine basılsın. Fotoğraf sahibi, geliştirici şifresi olmadan açılan `GMM Connect | Paylaşım isteği` penceresinden Kabul veya Reddet seçsin. Kabulden **önce hiçbir dosya adı iletilmez**.
5. Kabul edilirse yalnızca izin verilen `GMMShare` klasörünün uygun resim adları fotoğraf isteyen cihazda görünür. Listeden dosya seçilip `Seçilen fotoğrafı al` düğmesine basılsın.
6. Fotoğraf aktarımı parçalı şekilde yapılır, SHA-256 bütünlük doğrulaması yapılır. Başarılı olursa alıcıda geniş depolama izni varsa `/sdcard/Pictures/GMMReceived/`, yoksa uygulamanın özel `files/GMMReceived/` klasörüne kaydedilir.

## Önemli sınırlar

- Bir kabul işlemi yalnızca **tek fotoğraf** talebi için kullanılabilir; bir sonraki dosya için yeni istek gerekir. Oturum süre aşımına ve oda değişikliğine duyarlıdır.
- Yalnızca `Pictures/GMMShare` içindeki resimler listelenir. Karşı cihazın **tüm fotoğraf galerisi veya bütün dosyaları** uzaktan gezilmez; bu özellikle izinli paylaşım için sınırlanmıştır.
- Geliştirici menüsündeki eski **yerel fotoğraf gönderimi** ayrı bir özelliktir: orada kendi cihazının dosyaları gösterilir.
- Paylaşım talebi kendi kontrolünüzde olan cihazlar arasında kullanılmalıdır. Fotoğraf aktarımı ayrıca uçtan uca parola şifreli değildir; Photon aktarım şifrelemesi ile SHA-256 bütünlük kontrolü farklı güvenlik özellikleridir.
- GMM özel ağ kanalı hâlâ **iki fiziksel Android cihazda test edilmedi**. Bu ZIP kaynak koddur, derlenmiş APK veya `.so` dosyası değildir. Eğer oyunda `Photon GMM fotoğraf kanalı hazır` görünmüyorsa Logcat loglarını inceleyin.
