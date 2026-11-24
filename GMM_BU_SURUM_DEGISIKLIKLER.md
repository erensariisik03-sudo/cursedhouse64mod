# GMM test güncellemesi — 24 Kasım 2026

- Açılışta bir adet renkli oyun içi Toast benzeri bildirim gösterilir; çift Android/ImGui karışıklığı kaldırıldı.
- Oyun -> İsim/mesaj kopyalama: oda adı, Photon ham isimler ve son 30 görünür sohbet satırı, rich-text dahil tek dokunuşla panoya kopyalanır.
- Sohbet klavyesi: sohbet için Android Tamam / TMP OnSubmit ve kullanıcının klavye dışına dokunması üzerine oyunun normal SendMessage metoduyla gönderme denenir; cooldown ve boş mesaj kontrol edilir.
- Fotoğraf aktarımı: emülatörde gelen fotoğrafın kaydı için depolama hatası tanısı ve uygulama-özel klasöre yedek kayıt. Gönderici ve alıcı aralarında Photon veri akışı korunur.

Her iki cihazda test edilmeden gerçek cihaz uyumluluğu garanti edilmez.
