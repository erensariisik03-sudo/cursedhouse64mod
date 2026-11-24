# GMM Connect — Pictures / çoklu fotoğraf / tek onay

## Test akışı

1. İki cihazda yeni arm64 `libmultiplayermod.so` ve `MANAGE_EXTERNAL_STORAGE` iznini içeren APK manifesti bulunmalı.
2. Paylaşan cihaz Android sistem ayarlarından tüm dosyalara erişim iznini vermeli. Mod izni otomatik veremez.
3. İki cihaz aynı Photon odasına girsin. İsteyen cihazda geliştirici kilidi (`YOBAZALİ03`) açılsın.
4. GMM Connect'te hedef oyuncu seçilip liste isteği gönderilsin. Paylaşan cihazdaki **Kabul et** düğmesine sadece bir kez basılsın.
5. Paylaşan cihaz yalnızca `/sdcard/Pictures` dizininin doğrudan içindeki JPG/JPEG/PNG dosyalarının adlarını listeler (alt klasörleri içermez). En çok 100 dosya listelenir; dosya başına üst sınır 8 MiB'dir.
6. İsteyen cihaz fotoğraf satırlarını işaretleyip **Seçilen fotoğrafları al** düğmesine bassın. Her fotoğraf sırayla gönderilir, dosya başına yeniden onay istemez.
7. Paylaşan cihaz ekranda **Paylaşımı kapat** diyerek aktif izin oturumunu bitirebilir. Oturum en geç 15 dakika sonra veya oda/oyuncu değişince kapanır.
8. Fotoğraflar geniş depolama erişimi varsa `/sdcard/Pictures/GMMReceived`, yoksa uygulamanın özel `files/GMMReceived` klasörüne yazılır. Farklı fotoğraflar aynı ada çakışmaz.
9. Yerel cihazda görünen oda sohbeti (AI kapalı olsa da) geniş depolama izni varsa `/sdcard/gmmod/chat_YYYYMMDD.log` dosyasına eklenir. Daha önce silinmiş/görünmeyen mesajlar geri getirilemez.
10. Oda adı ve ham Photon takma adları (etiketler dahil) kopyalama panelinden panoya kopyalanabilir.

## Açılış bildirimi

- Standart Android Toast kısa bir `DiCaprio Mod` mesajı gösterir.
- Kırmızı/turuncu/sarı yazı Android 11+ sistem Toast'u yerine oyun içi ImGui üst katmanında gösterilir.
- İzin verilmeden geri dönülürse oyun, bazı özelliklerin kullanılamayacağı uyarısını Toast ile gösterir.

## Teknik sınırlar

- 256 KB prototip sınırı kaldırılmış; kesintisiz ve sınırsız dosya aktarımı **yok**, maksimum 8 MiB/fotoğraf.
- Aynı izin oturumu içinde aktarım sırayla ve tek dosya in-flight yapılır. Aynı `browse_id`, oturuma ait Photon aktör kimliğiyle doğrulanır.
- Geliştirici kilidi yalnızca alıcı ekranı için gereklidir; karşı cihaz, geliştirici kilidini açmadan paylaşım isteğine onay verebilir.
- APK'nin AndroidManifest.xml dosyasında geniş depolama izni yine ayrıca bulunmalıdır. DEX dosyaları bu yamada değiştirilmedi.
- Yerel testler geçti; iki cihazda gerçek 8 MiB/çoklu dosya aktarımı henüz doğrulanmadı.
