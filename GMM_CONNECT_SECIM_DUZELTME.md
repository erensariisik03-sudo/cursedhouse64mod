# GMM Connect — Fotoğraf seçme/indirme düzeltmesi (2026-11-24)

**Log:** `RX G/L/E` ile liste geliyor; `fetch pressed ... selected=0` ve `TX kind=F` yok.

**Kök sorun:** İndirme komutuna gelindiğinde seçili uzak dosya boş. Ayrı scroll/child içinde bulunan fotoğraf satırının dokunması veya kullanıcının yalnızca alt indirme düğmesine basması bu duruma yol açabiliyordu.

**Düzeltmeler:**
- Liste bitince **tek fotoğraf varsa otomatik seçilir**; yine de indirme için oyuncunun kendi dokunması gerekir.
- `Fetch` işlenirken tek listelenmiş fotoğraf seçimi kaçırılmışsa tekrar otomatik tamamlanır.
- Birden fazla fotoğrafta seçim **otomatik yapılmaz**; fotoğrafı seçmek/indirmek için `Al: <dosya>` satırına dokunulur.
- Uzak dosya düğmeleri iç içe scroll child yerine menünün ana kaydırılabilir panelinde çizilir, böylece dokunuşlar gizlenmez.
- Loglar `GMM photo browse listing complete count=... auto_selected=...` ve `GMM photo UI: fetch pressed ... files=...` ile seçim durumunu açıklar. Dosya adları loglanmaz.
- Photon TX/RX, şifre kilidi, gönderici onayı ve yalnızca paylaşılan `Pictures/GMMShare` kapsamı korunur.

**Kurulum:** Yamadaki `src/keyboard_ui.cpp`, `src/photo_transport.cpp` ve isteğe bağlı `tests/keyboard_ui_test.cpp` dosyalarını aynı konuma yerleştirip GitHub Actions ile ARM64 `.so` oluşturun. İki cihazda güncel `.so` kullanın. DEX değişikliği yok.

**Test:** Geliştirici şifresini girin, GMM Connect'te hedef oyuncudan fotoğraf listesi isteyin. Karşı cihaz onaylasın. Tek dosya varsa doğrudan `Seçilen fotoğrafı al`; birden çok dosya varsa `Al: <dosya>` satırı. `TX: protocol=2 kind=F ok=1` sonra vericinin `RX kind=F` loglarını kontrol edin.

**Yerel doğrulama:** ImGui dokunmatik UI testi 5 ekran boyutunda PASS; photo_browse_protocol, photo_transfer_protocol, developer_lock PASS; Android photo_transport C++ sözdizimi PASS. Gerçek Photon cihaz testi yapılmadı.
