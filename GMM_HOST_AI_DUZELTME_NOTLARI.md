# GMM — Yerel Host Modu ve AI mesaj tekrarı düzeltmesi

- Geliştirici > Oda Yönetimi > **Yerel Host Modunu Aç/Kapat**: `PhotonNetwork.get_IsMasterClient` yerel getter için Main(1).cpp tarzı true override. Yalnızca bu getter; bellek offsetlerine yazma yok. **Sunucu MasterClient değişmez**. Her oda/şifre oturumu sonlandığında kapanır.
- `Main(1).cpp` içindeki `OnMasterClientSwitched` `RemovePlayer` çağrısı taşınmadı. `RemovePlayer` bu yerel true override için gerekli değil, başka oyuncuyu etkiler. `SetMasterClient` yöntemi ayrı duruyor.
- İnsan tarafından gönderilen sohbet metni kutuda kalır; AI, sesli AI, sistem ve oyun duyuruları otomatik olduğundan bunların taslağı korunmaz.
- `canSendMessage` alanı artık hiçbir genel SendMessage hook'unda zorla `true` yapılmaz. AI normal oyun bekleme süresine uyar. Sistem duyurularındaki kontrollü geçici izin kendi kodunda kalır.
- **Test gerektirenler:** Gerçek cihazda yerel host kontrolünün beklenen oyun arayüzlerini açıp açmadığı, AI tek mesaj gönderimi ve hostluk yetkilerinin odadaki diğer oyuncularla tutarlılığı.
