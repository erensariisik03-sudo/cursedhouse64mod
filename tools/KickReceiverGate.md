# Oyuncu atma: sohbet şakası

Güncel sürüm gerçek oyuncu çıkarmaz. Eski setupPlayerRoomItem fgu/fgv hook'ları,
PhotonNetwork.CloseConnection çözümü/çağrısı, EnableCloseConnection değişikliği
ve ayrılma doğrulama aşamaları mod kodundan kaldırıldı.

Oda ve normal sohbet bağlantısı yeterlidir; IsMasterClient şartı yoktur.
Başlatan oyuncunun otomatik +1 eveti ve ilk oy kuralı korunur. Oylar isim yerine
onaylı güncel Player kimliğinden alınır; aynı isimli oyuncular karışmaz.

Evet çoğunluğunda önce “... adlı oyuncu atılıyor...” yazılır. Normal sohbet
bekleme süresi izin verdiğinde, en erken 1 saniye sonra “Şaka şaka, ... adlı
kişiyi atmıyoruz ama adam olsun.” gönderilir. Eşitlik/hayır şakayı başlatmaz.
Kullanıcının taslağına/fokusuna müdahale edilmez; oda değişimi oylamayı bitirir.

Host/client, duyuru sırası, taslak/fokus, sohbet beklemesi, eşitlik ve oda
kaybı sahte IL2CPP bütünleşme testinde kapsanır. Gerçek Android denemesi gerekir.
