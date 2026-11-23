AndroidManifest.patched.xml: orijinal ikili manifest + <queries><intent><action android:name="android.intent.action.TTS_SERVICE"/></intent></queries>
(Android 11+ paket görünürlüğü: oyunun sistem TTS motorunu görmesi için).
APK'ya geri koyduktan sonra yeniden imzala (zipalign + apksigner).
Tekrar üretmek: python3 patch_axml.py <orijinal.xml> <cikti.xml> ; doğrulama: python3 dump_axml.py <xml>
