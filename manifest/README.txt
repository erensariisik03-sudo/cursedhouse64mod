AndroidManifest.patched.xml: oyunun ikili (AXML) Android manifesti.
Bu sürüm, mevcut Android bileşenlerini ve TTS_SERVICE paket görünürlüğünü koruyup
android.permission.MANAGE_EXTERNAL_STORAGE beyanını içerir.

ÖNEMLİ: Geniş dosya erişimi yalnız manifest ile verilmez. Android 11+ kullanıcının
sistem ayarından ayrıca açık onayı gerekir. DEX üzerinde Activity öncesi izin ekranı değiştirilmedi. Güncel kaynak native
Unity başlangıcında izin yoksa Android sistem izin sayfasını açar.
Geliştirici menüsünde depolama izni düğmesi bulunmaz.

APK'nin manifestini değiştirince zipalign + apksigner ile yeniden imzalayın.
Mevcut patch_axml.py yalnız eski TTS hizmeti sorgusunu ekleyen tarihsel betiktir;
bu dosyayı yeniden üretmek için kullanırsanız MANAGE_EXTERNAL_STORAGE iznini
ayrıca yeniden eklemeniz gerekir.

Doğrulama: python3 dump_axml.py AndroidManifest.patched.xml
