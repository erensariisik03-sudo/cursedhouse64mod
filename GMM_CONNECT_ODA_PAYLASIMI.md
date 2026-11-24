# GMM Connect – Oda paylaşımı

Bu sürümde güvenilir oyuncu seçme arayüzü kaldırıldı. Kullanıcı kendi cihazında **Oyun > Fotoğraf paylaşımı > Bu odada paylaşımı aç** düğmesine basmadan karşıdan dosya listesi veya dosya alınamaz.

Paylaşım açıkken **aynı Photon odasındaki bütün modlu oyuncular** `Pictures` klasörünün doğrudan içindeki desteklenen görselleri (mevcut dosya sınırlarıyla) isteyebilir. Her istek veya fotoğraf için ayrıca onay penceresi yoktur. Kullanıcı **Paylaşımı kapat** düğmesine basarsa onaylı aktarım ve paylaşımlar iptal edilir. Oda değişince veya odadan çıkılınca paylaşım kapalıya döner.

Geliştirici kilidi yalnızca alıcı arayüzünün erişimini kısıtlar; başka istemciler için kriptografik kimlik doğrulaması değildir. SHA-256 bir hash algoritmasıdır; sabit kısa bir parolayı 256 bit rastgele anahtara dönüştürmez.

Değişen kaynaklar: `src/photo_transport.cpp`, `src/photo_transport_bridge.h`, `src/keyboard_ui.cpp`; testler: `tests/photo_batch_bridge_test.cpp`, `tests/keyboard_ui_test.cpp`. Fotoğraf aktarım protokolü/Photon mesaj türleri değiştirilmedi.
