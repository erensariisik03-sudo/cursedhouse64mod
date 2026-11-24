# GMM Connect — sessiz, oturum bazlı paylasim

Bu sürüm fotoğraf isteği geldiğinde açılan iki pencereli Kabul Et / Reddet arayüzünü kaldırır. **Cihaz sahibi**, oyun içindeki **Oyun > Fotoğraf paylaşımı** bölümünden aynı Photon odasındaki bir oyuncuyu seçerek o oyuncuya **bu oda boyunca** `Pictures` içindeki uygun JPG/PNG fotoğraflarına erişim izni verir. Bunun ardından seçilmiş oyuncunun listeleme ve indirme istekleri ikinci bir pencere veya Toast olmadan işlenir.

**Varsayılan kapalıdır:** Yerel cihazda güvenilir oyuncu seçilmemişse fotoğraf listesi istekleri otomatik reddedilir. Yetki sadece aynı odadaki seçilmiş Photon aktör numarası için geçerlidir. Başka odaya geçilince, sahne yeniden yüklenince, seçili oyuncu ayrılınca veya **Paylaşımı kapat** seçilince yerel izin iptal edilir. Sonradan gelen istekler tekrar reddedilir.

İsteyen tarafta **AI > Geliştirici > GMM Connect** ve geliştirici şifresi gereklidir. Fotoğrafı paylaşan tarafta geliştirici şifresi gerekmez.

Bu bir APK / DEX güncellemesi değildir; ARM64 native kaynak yamasıdır. Sistem Android'in `MANAGE_EXTERNAL_STORAGE` iznini otomatik vermez veya aşmaz. Gerçek telefon–emülatör aktarımı yeni sürümle henüz doğrulanmamıştır.

## Kullanım

1. İki cihazla aynı Photon odasına gir.
2. Fotoğrafı verecek cihazda **Oyun > Fotoğraf paylaşımı** bölümüne gir ve dosyaları isteyen oyuncunun isminin yanındaki **Paylaşımı aç** düğmesine bas.
3. Fotoğrafı alacak cihazda geliştirici şifresiyle **GMM Connect** aç, oyuncuyu seç ve fotoğrafları iste.
4. Aynı odada paylaşım açıkken istek penceresi çıkmaz. İş bitince gönderen cihazdaki **Paylaşımı kapat** düğmesine bas.

## Kapsam

- Sadece `/sdcard/Pictures` klasörünün **doğrudan** içindeki JPG/JPEG/PNG dosyaları (alt klasörler taranmaz).
- Dosya başına 8 MB sınırı ve en fazla 100 dosyalık liste sınırı korunur.
- Verici açıkça bir oda oyuncusunu seçmeden uzaktan dosya listesi alınamaz.
- Paket kodlaması ve fotoğraf aktarma protokolü değiştirilmemiştir.
