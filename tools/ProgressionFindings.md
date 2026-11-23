# Progression bağlantıları

Kaynak: verilen dump.cs ve ARM64 libil2cpp.so. Aşağıdaki adlar bu dosya
sürümünden; uygulama adres/offset kullanmaz.

- GameDifficulty byte enum: Easy=0, Normal=1, Hard=2, Extreme=3,
  Impossible=4, Practise=5. PhotonRoomsManager.Difficulty aynı enum.
- Yeniden doğma süresi özelliği kullanıcı isteğiyle kaldırıldı. GrannyManager /
  GrandpaManager dönüş süresi işlevlerine kanca kurulmaz; native bekleme korunur.
- EnemyAIGrannyMulti.eik ve EnemyAIGrandpaMulti.dum önce sırasıyla
  eim/eil/ein ve duo/dun/dup çağırır. Bunlar durum/rota hazırlığıdır;
  canlı zorluk yenilemesinde köklü scoped engelleme gerekir. Devamındaki
  native switch hız/animasyon değerlerini yazar. Start tekrarlanmaz.
- SpiderMomAIController.imw rota listesine ekler ve kendi difficulty/hız
  alanlarını günceller. Aynı listeyle tekrar çağrı yapılmaz: geçici List,
  native kurulum, özgün listeyi geri yükleme uygulanır.
- HandlePickUpItemInteractions.hqw raycast, mesafe, ön koşul, gereken eşya
  listesi ve native etkileşim işlemlerini yürütür. currentObjectInHand byte
  enumdur, rgn baktığı nf nesnesidir.
- WorldInteractable.fge gerçek requiredItems listesini verir; fgj eşya
  yerleştirme/tüketme seçeneğidir. fgd normal olayı yayınlar. Native genel
  etkileşim kodu bu olayı kendi RPC yoluna taşır. Mod doğrudan sahte RPC
  alıcısı veya PhotonMessageInfo üretmez.
- fge/fgj getter gövdeleri iki ARM64 komutudur. DobbyHookNear yalnız bir
  dört-bayt B yazar ve ilk komutu taşır. Yer bulunamazsa normal 16-bayt
  trampoline'a geçmez; komşu getter/RET korunur.

Cihaz testi gerektirenler: Her WorldInteractable presetinin gerçek kilit/
animasyon/ağ sonucu, belirli klonun native ölüm yöneticisine bağlı olup
olmadığı, modsuz alıcıdaki görünüm. Özel backend, ağ kimliği taklidi veya
alıcı izin kapısını değiştirme eklenmedi. Atma sorunu bu değişiklikle çözülmedi.
