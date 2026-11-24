# GMM Fotoğraf Transferi — 1. Aşama (şifre kilidi ve test çekirdeği)

## BU SÜRÜMDE NELER ÇALIŞIYOR?

- Genel AI menüsüne **Geliştirici** sayfası eklendi.
- Geliştirici sayfasından alıcı için en az 8 karakterlik şifre kurulabiliyor.
- Şifre yalnızca rastgele 16 bayt salt + PBKDF2-HMAC-SHA256 (100000 tur) doğrulama değeri olarak uygulamanın **özel files** dizininde `gmm_photo_gate.v1` adıyla saklanır (0600).
- Şifreyi doğrulamadan Dosya Alma Modu açılamaz; oyun yeniden açılınca kilitli başlar.
- En fazla 5 başarısız şifre denemesi kabul edilir (sonrasında yeniden başlatma gerekir).
- Taşıma çekirdeği: JPEG/PNG imza kontrolü, 256 KiB boyut sınırı, 768 bayt parçalar, SHA-256 bütünlük kontrolü, sıra / tekrar / hatalı aktör reddi, alıcı kilidi ve oda aktörü eşleştirmesi.
- `tests/photo_transfer_security_test.cpp` ile kriptografi test vektörleri ve taklit paketler doğrulandı.

## BU SÜRÜMDE HENÜZ YOK

**Gerçek ağdan fotoğraf gönderme/alma HENÜZ ETKİN DEĞİL.** Yeni UI'da da bu açıkça yazıyor. Aşağıdakiler bağlanmadan iki telefon arasında gerçek fotoğraf gönderilmiş sayılmaz:

1. Photon/PUN2 `RaiseEvent` + doğrulanmış `OnEvent` alıcısı; olay kodu başka oyun olaylarıyla çakışmamalı.
2. Modlu cihaz keşfi/uyumlu sürüm ve gönderici/alıcı Photon aktör numarası doğrulaması.
3. Tek fotoğraf için `offer → alıcı kabul → parça aktarımı → bütünlük doğrulaması` protokolü, zaman aşımı ve paket başına hız sınırı.
4. Fotoğrafın uygulama tarafından seçimi: Android Photo Picker / `ACTION_OPEN_DOCUMENT`; **genel depolama izni istemeye gerek yok**. Böylece ilk açılışta tüm fotoğraflara erişim alınmaz.
5. Alıcının onayladığı dosyayı kullanıcıya gösterecek uygulama içi güvenli galeri/dışa aktarma akışı.
6. Gizlilik gerekiyorsa ayrıca uçtan uca şifreleme: **SHA-256 dosya içeriğini şifrelemez.** Alıcının yerel şifresini başka cihazlara yayınlamayın.

## ÖNERİLEN AĞ PROTOKOLÜ

- Özel olay sınıfı ve sürüm tanımlı türler: HELLO, OFFER, ACCEPT, CHUNK, DONE, CANCEL.
- Başlangıç: modlu istemciler yalnızca aynı Photon odasında görünür, alıcı *önceden şifre doğrulamış ve almaya izin vermiş* olmalı.
- Pakette alıcı aktör numarası, rastgele transfer kimliği, toplam bayt/parça sayısı, bütün dosyanın SHA-256 değeri bulunmalı.
- `EventData.Sender` gerçek aktörü Photon üzerinden okunmalı; payload içindeki gönderen bilgisine güvenilmemeli.
- Doğrudan ilgili aktöre gönderilmeli; cached event olmamalı; oda değişiminde teklif ve parçalar silinmeli.
- Alıcı kullanıcının her transferi ayrıca onaylaması gerekir.
- **100% başarı** demek için iki gerçek ARM64 cihazda gönderim, kesinti, büyük dosya, bozuk paket ve aynı anda bağlantı değişimi test edilmeli.

## KURULUM

Bu paket native kaynak değişiklikleri içerir; kendi GitHub projenizdeki mevcut dosyaların üzerine klasör yapısıyla koyun, GitHub Actions ARM64 derlemesi alın. APK içine yeni manifest izinleri veya DEX kodları **henüz gerekmiyor**; gerçek fotoğraf seçimi eklenecek sonraki aşamada da Photo Picker genel depolama izni gerektirmeyecek.

Bu ara sürüm, henüz bulunmayan Photon haberleşme kodunu gizlice etkinleştirmek yerine mevcut GMM/SEDA özelliklerini korumayı amaçlar.
