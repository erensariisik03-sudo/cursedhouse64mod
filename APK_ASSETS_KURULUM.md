# GMM APK Assets kurulum rehberi

Bu proje `assets/gmm_seda` modellerini APK'ye gömme sürümüdür.

1. GitHub Actions ile yeni ARM64 `libmultiplayermod.so` derle. **Eski .so çalışmaz.**
2. APK'nin köküne `assets/gmm_seda/` klasörünü olduğu gibi kopyala.
3. İçinde `asset-version.txt`, `tokens.txt`, `c32/encoder.int8.onnx`, `c32/decoder.onnx`, `c32/joiner.int8.onnx` bulunmalı.
4. Yeni derlenen .so ve sherpa/ONNX paylaşımlı .so dosyalarını `lib/arm64-v8a/` içine yerleştir.
5. `assets/bin/Data/datapack.unity3d` dosyasına dokunma. APK'yi yeniden imzala ve kur.
6. Logcat filtresi: `adb logcat -d -s multiplayermod:I sherpa-onnx:W` veya `adb logcat -d | Select-String 'GMM_SEDA_ASSETS|sherpa-onnx|SEDA'`.

İlk transkripsiyon denemesinde assetler dahili `files/gmm_seda` dizinine otomatik çıkarılır.
Android native sherpa-onnx doğrudan `assets/...` yolunu sıradan dosya gibi açamaz.
Telefonun SD kartına dosya kopyalamak zorunda değilsin.

Not: Bu ZIP yalnızca **kaynak + APK asset dosyalarını** içerir; hazır derlenmiş
APK veya yeni ARM64 .so içermez. Build alıp APK'ye yerleştirmek gerekir.
