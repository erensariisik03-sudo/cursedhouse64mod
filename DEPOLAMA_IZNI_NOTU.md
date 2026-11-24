# GMM — Depolama erişimi güncellemesi

## APK manifest

APK'nin kökündeki **AndroidManifest.xml** dosyasını, paketteki `APK_ici/AndroidManifest.xml` ile değiştirin. Bu Android binary XML dosyasıdır; metin editörüyle açılan düz XML değildir.

Mevcut izinler ve uygulama bileşenleri korunarak yalnızca `android.permission.MANAGE_EXTERNAL_STORAGE` izni eklenmiştir.

## Geliştirici menüsü

Geliştirici > Depolama erişimi: AÇIK/KAPALI düğmesi Android 11+ üzerinde uygulamanın Tüm dosyalara erişim izin sayfasını açar. Buton izni otomatik vermez. Kullanıcı sistem ekranında açıkça izin verir. Ayara dönülünce düğme mevcut durumu okur.

Yalnızca bu izin, ne uygulama açılır açılmaz otomatik dosya taraması yapar ne de Photon üzerinden dosya gönderimini etkinleştirir. Özel dosya aktarımı güvenlik/şifre prototipi mevcut pakette olduğu hâliyle korunmuştur; gerçek ağ gönderim/alım henüz entegre edilmemiştir.

`MANAGE_EXTERNAL_STORAGE` geniş kapsamlıdır; Google Play politikalarında çoğu oyun için kullanımına izin verilmez. Yalnızca kullanıcı tarafından bilinçli olarak onaylanmış kullanım senaryolarında uygulayın.

## Kaynak değişiklikleri

- `src/storage_access.h` + `src/storage_access.cpp`: Android sistem izin sayfasını açma ve izin durumunu sorgulama
- `src/keyboard_ui.cpp`: geliştirici menüsüne izin düğmesi; ayrıntılı teknik açıklamanın kaldırılması
- `src/unity_keyboard.cpp`: JNI VM bağlantısı
- `CMakeLists.txt`: ARM64 kütüphane derlemesine yeni C++ modülü eklenmesi

## Dikkat

APK değiştirildiğinde yeniden imzalanmalı; mevcut imzadan farklı bir imzayla üstüne kurulum mümkün olmayabilir. Kurulu uygulamanın verilerini yedeklemeden kaldırmayın. Gerçek Android cihazda ayar ekranının açılması ve ARM64 derlemesi henüz doğrulanmadı.


## Bu kaynak sürümü (geliştirici kilidi)
- Geliştirici menüsü sabit parola doğrulaması olmadan açılmaz; oluşturma/değiştirme menüsü yoktur.
- Parola her uygulama sürecinde yeniden istenir; 5 hatalı denemeden sonra süreç yeniden başlatılana dek kilitlenir.
- Depolama erişimi düğmesi geliştirici menüsünden kaldırıldı; fotoğraf teklif/kabul menüsü korundu.
- Uygulama ilk Unity ana-kare çağrısında Android 11+ için `Environment.isExternalStorageManager()` kontrol eder; izin yoksa Android'in **açık onay gerektiren** sistem izin sayfasını bir kez açar.
- **classes8.dex değiştirilmedi.** Bu native başlangıç yöntemi oyun ana menüsünü engellemez; Android Activity açılmadan önce engelleme ve izin reddinde kapanış hedefleniyorsa ayrıca gerçek APK/Activity entegrasyonu gerekir.
- APK manifestinde `android.permission.MANAGE_EXTERNAL_STORAGE` bulunmalı ve APK yeniden imzalanmalı.
- Sabit parola özeti (SHA-256) tek başına güçlü bir dağıtım/cihaz eşleştirme sırrı değildir, APK tersine mühendislikle incelenebilir.
