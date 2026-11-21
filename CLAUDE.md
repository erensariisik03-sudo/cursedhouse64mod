# Proje geliştirme notları

Önce README.md dosyasını okuyun. Android çıktısı yalnız arm64-v8a için
libmultiplayermod.so dosyasıdır. Derleme GitHub Actions üzerinden yapılır.

- C++17, OpenGL ES 3, statik C++ çalışma zamanı ve 16 KiB ELF hizalamasını koruyun.
- Dışarı yalnız mm_* ve JNI_OnLoad sembolleri açılır.
- IL2CPP metotlarını ve alanlarını çalışma anında metadata/API üzerinden çözün;
  dump dosyalarından sabit RVA veya alan offset'i taşımayın.
- Yönetilen çağrıları Unity'nin betik iş parçacığında yapın. Başlangıç iş
  parçacığını VM'ye bağlamayın. Gerçek bir Unity karesi gözlenmeden başlatmayın.
- İş parçacıkları arasında kopyalanmış değerleri ve kilitli köprüleri kullanın;
  yönetilen nesne referanslarını GC handle ile koruyun.
- Oyun içi TMP alanlarında Android klavyesini açık bırakın. ImGui yalnız kısayol
  işlemleri gönderir; renderer metin kopyasıyla native yazıyı ezmeyin.
- 1024 Unicode karakter sınırını değişiklik/gönderme callbacklerinden önce koruyun.
  İmleç/seçimde UTF-16 ve UTF-8 dönüşümünü doğru yapın.
- Klavye entegrasyonu hazır değilse oyunun kendi klavyesini açık bırakın.
  Aktif metin alanının ayarlarını oturum bitince geri yükleyin.
- Metin, parola, sohbet içeriği veya API anahtarını loglamayın. Mevcut özel
  ayar dosyasını koruyun; ağ isteğinde anahtar HTTP başlığında gönderilir.
- Native isim kontrolünde özgün alt sınırı koruyun; belirsiz komut örüntülerini
  reddedin. Observer yalnız AArch64 doğrudan tail branch zincirini izler;
  komşu API girişleri üzerine yazmayı reddeder.
- Kıyafet/dans/sohbet için mevcut oyun akışını ve yerel sahiplik kontrollerini
  koruyun. Yeni ağ paketleri, tahmini RPC veya bulut/satın alma çağrıları eklemeyin.
- Kullanıcının isteğiyle Oyun paneli gameplay modülüne bağlanmıştır. Taban
  system-terminal-testfix sürümüdür; kıyafet ağ inceleme denemesi dahil değildir.
  Eski düşman dosyasındaki RVA/offset'leri taşımayın. Grandpa'nın güncel kendi
  sınıf/alanlarını çözün. Enemy alanı yazmadan önce Single tipini doğrulayın.
  Eşya prefablarını kendi aktif envanterindeki HandItemTie kayıtlarından alın;
  oyun kaynak yolu çözücüsü ve normal Instantiate yolunu koruyun. Ağdan kaldırma
  yalnız sahipliği doğrulanan nesneye uygulanır; diğerinde yalnız yerel kaldırma vardır. Nesne
  kaldırmayı normal ölüm/ödül akışı diye sunmayın. God Mode yalnız köklü kendi
  oyuncu/manager nesnesi için, aynı oda/sahnede ve Unity thread'inde engeller;
  güncel HandleEnemyAttacked/PlayerStateManager yapısında da kendi hedefini
  doğrulayın; başka hedef ve Dead dışındaki durumları iletin. Diğer tüm
  çağrıları asıl metotlara iletin. Coroutine MoveNext ABI'sini koruyun.
  Dokunma callback'lerinde render karesinin yerel değişkenlerini referansla
  yakalamayın; oturum, seçim ve komut değerlerini kopyalayın.
  Scene.handle opak, işaretli bir kimliktir; pozitiflik koşulu koymayın.
  Geçerlilik/yüklenmeyi Unity API'siyle sınayın. Kendi oyuncunu keşfederken
  Behaviour.enabled yerine GameObject.activeInHierarchy ve Photon sahipliği
  arayın; özel view önbelleği boşsa MonoBehaviourPun.get_photonView kullanın.
  Düşman/eşya/God kontrollerinin hedef koşullarını ayrı tutun ve pasiflik
  nedenini gösterin; bekleme durumundan hazır durumuna geçişte yazıyı yenileyin.
- Dobby bu paket için uyarlanmıştır. Bağımlılık lisanslarını ve ImGui'nin
  mevcut context kaybı uzantısını koruyun.
- Kullanıcı açıkça istemedikçe yerel build veya C++ test derlemesi çalıştırmayın.
  Eko modda bildirilen hatanın kaynağını düzeltin; ek kontrolü yalnız UTF-8
  editörüyle sınırlayın. ZIP sağlayın; derleme GitHub
  üzerinden kullanıcı tarafından yapılır. Yapılmayan doğrulamayı yapılmış göstermeyin.

- SKIP yalnız iç kontrol işaretidir; model yanıtını parçalamadan ve sohbete
  göndermeden önce eleyin. Hazır selamları AI'ye göndermeyin, gerçek soruyu
  selam eşleştirmesiyle yutmayın. İsim hatırlatmaları yalnız bot/isim süzgeci
  açıkken çalışır; normal yanıtlar, taslaklar ve oyun bekleme süresi önceliklidir.

- Kullanıcının son isteğiyle yerel gönderim izni tüm ChatManager.SendMessage
  çağrılarına uygulanır; bayrağı yalnız çağrı boyunca değiştirip geri yükleyin.
  Durum akışını 6 satır/sn ve 5 sn ile sınırlayın; gerçek uygulama ayrıntıları
  yerine kurgusal terminal satırları ve anlaşılır Türkçe sonuç yazın.
- Terminal gösterimi açılışta 24, diğer olaylarda 18 satırdır. Kernel/çekirdek
  derleme metni kullanmayın; uzun satırlarda kurgusal konsol servisi akışını anlatın.
- Yalnız terminal gönderiminde PhotonNetwork.get_NickName okumasını köklü
  System metnine yönlendirin; kalıcı oyuncu özelliğini/adını değiştirmeyin.
  İstek iş parçacığına özeldir, SendMessage girişinde tüketilir ve iç içe normal
  gönderime aktarılmaz. Kanca yoksa gösterimi atlayın. Photon aktör kimliği
  değişmediğinden bu özelliği sunucudan kimlik gizleme diye sunmayın.

- Kullanıcının son isteğiyle düşman ayarlarında genel oda sahibi kilidi kaldırıldı.
  Yerel etki ile ağ yetkisini ayırın; tüm istemcilere başarı garantisi vermeyin.
  Gerçek ölüm, nesne kaldırma ve klonlama ayrı işlemlerdir. Shotgun ölüm isteği
  yalnız doğrulanan Granny/Grandpa RPC isimleriyle normal SDK'ya gider; Sender
  veya PhotonMessageInfo üretmeyin. Klonlar gerçek prefab ve yeni SDK kimliği
  kullanır. PhotonView içeren sahne nesnesini Unity.Instantiate ile kopyalamayın.
- Eşya kuralı: boş elde yeni eşya ele; elde aynısı varsa yeni kopya yere; farklı
  eşya istenirse önce eldeki normal akışla bırakılır. Mermi ve şırınga yere
  oluşturulur. Bir saniyelik yerel bekleme kaldırıldı; en çok kare başına bir
  oluşturma işlenir. Eşya listesinde sürükleme seçim üretmemelidir.
- Durdurma sırasında ana/alt Animator ve Animation.enabled değerlerini saklayıp
  kapatın; devamda saklanan durumu geri yükleyin. Crow gerçek görsel kökleri
  ölçeklenir; iç içe kökleri tekrar çarpmayın. Crow üst sınırı 20x'tir.

- Eşya adları kullanıcının son tercihiyle İngilizcedir. PickUpObject enum kimliği
  etiketle eşleştirilir; gerçek prefab/resource anahtarı etiketle değiştirilmez.
- God Mode kendi hedefinin DaysManager.foq ölüm/gün kaydını, kendi öldürme
  kamerasını ve doğrulanan saldırı başlangıçlarını korur. Diğer hedef/kaçış,
  mod kapalı hali ve oda/sahne değişimi özgün akışa iletilir; gün saati genel
  olarak dondurulmaz. Modsuz uzak yöneticinin kararını iptal ettiğini iddia etmeyin.
- Hayalet modu yalnız sahipliği doğrulanan FirstPersonController.hjs ve kendi
  düşme kontrolüne uygulanır. Joystick yatay hareketi korunur; kapanınca normal
  hareket ve sıfırlanmış dikey hız döner. Başlangıca dönüş komutu oturumludur.
- Infinity Ammo yalnız eldeki kendi Crossbow/Shotgun atışında yük bayrağını
  çağrı boyunca hazırlar; özgün atışı bir kez çağırıp gerçek durumu korur.
  Ortak ShotgunManager bayrağını kalıcı değiştirmeyin, cooldown'u kaldırmayın.
- Klon oyuncunun kamera yönünde yatay ve dik yerleştirilir; yürünebilir zemin,
  AI bileşeni ve yeni hiyerarşinin ViewID'si doğrulanır. Geçerli ön zemin yoksa
  yana/başka yere sessizce oluşturmayın. Mevcut ana manager referansı ezilmez.
- Shotgun Ammo kaynak hazır değilken de gri listelenir. Yerel model gerekiyorsa
  yalnız script/PV içermeyen görseli veya gerçek mesh/materyallerle oluşturulan
  yeni görseli kullanın. PhotonView/script kopyalayıp Awake çalıştırmayın.
  Kendi oyuncu yaklaştığında normal hra(Ammo) ile doldurmayı doğrulayın;
  kaynak nesneyi ve başka oyuncunun mühimmatını tüketmeyin.
