# GMM Connect - Tek dokunusla fotograf alma yamasi (2026-11-24)

Loglarda TX R ok=1, RX G/L/E gelmesi, karsi taraftaki dosya listesinin alindigini gosteriyor.
Fotografi alma sorununda TX F logu yok; yani dosya alma istegi Photon'a verilmeden once UI komutu/selection asamasinda takiliyor.

Degisiklikler:
- Liste satirlari 'Al: <dosya>' seklinde; satira dokunmak secim + indirme istegini tek komutta kuyruga alir.
- 'Secilen fotografi al' dugmesi halen mevcut; secim yapilmamis olmasi dugmeyi pasiflestirmez; eksik secimde durum mesaji verir.
- Dosya adi ve fotograf icerigi loglanmaz. Yeni loglar: `GMM photo UI: remote choose ...`, `GMM photo UI: fetch pressed ...`, `GMM photo TX: protocol=2 kind=F ...`.
- Verici onayli dosyayi acamazsa sessizce bekletmek yerine olumsuz yanit doner.
- Developer kilidi, karsi taraf onayi ve GMMShare erisim siniri korunur.

Cihaza kurulum: GitHub Actions ile yeni arm64-v8a libmultiplayermod.so derleyip her iki cihaza kurun. DEX degisikligi yok.

Test: iki cihaz ayni Photon odasina girer. Alici geliştirici sifresiyle GMM Connect'i acar, oyuncuyu secer, fotograf listesi ister. Karsi cihaz istegi kabul eder. Alici fotograf satirinda 'Al: ...' dugmesine dokunur.

Beklenen log sirasi: TX protocol=2 kind=F ok=1 (alici); RX protocol=2 kind=F (verici); TX protocol=1 kind=O (verici); RX protocol=1 kind=O (alici). Ardindan transfer ve kayit sonucu.

Yerel testler: ImGui menu/touch testi 5 boyutta PASS, image codec/security, consent protocol ve sabit kilit PASS, Android stub C++ syntax PASS. Gercek iki cihaz testi yapilmadi.
