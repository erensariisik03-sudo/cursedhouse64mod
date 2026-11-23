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
- Durdurmak için AI Behaviour.enabled değiştirmeyin. Karar Update/FixedUpdate
  ve saldırı tetikleyicisini kendi köklü aktöründe oturumla bekletin; OnDisable
  olay/coroutine akışını bozmayın. Alt animasyon ve NavMesh hızını saklayın;
  durdururken hızı her kare sıfırlayın ve yalnız hedefin konum/dönüş yazılarını
  sabitleyin. Agent kapalıyken bekleyen hız geri yükleme kaydını silmeyin.
  Crow gerçek görsel kökleri
  ölçeklenir; iç içe kökleri tekrar çarpmayın. Crow üst sınırı 20x'tir.

- Eşya adları kullanıcının son tercihiyle İngilizcedir. PickUpObject enum kimliği
  etiketle eşleştirilir; gerçek prefab/resource anahtarı etiketle değiştirilmez.
- God Mode kendi hedefinin DaysManager.foq ölüm/gün kaydını, kendi öldürme
  kamerasını korur; kendi oyuncuya vuruş girişleri/RPC alıcıları engellenir. Düşman animasyonu kendi controller akışındadır. Diğer hedef/kaçış,
  mod kapalı hali ve oda/sahne değişimi özgün akışa iletilir; gün saati genel
  olarak dondurulmaz. Modsuz uzak yöneticinin kararını iptal ettiğini iddia etmeyin.
- God korumasında düşmanı dontHitPlayer ile kilitlemeyin. Yalnız yerel gerçek
  master, kendi korunan oyuncusuna yönelen Granny/Grandpa/MomSpider'ın
  saldırı animasyonuna klip süresi/pay veya 3,5 sn fallback ile izin verir;
  sonra takılan caught/attacking/eating mandalını bırakır. Takip/hedef korunur;
  devriyeye zorlamayın. AnimatorStateInfo API'leri bu dump'ta yoktur. Güncel Photon oyuncu hedefi
  başka biri ise eski yerel transform referansına rağmen değişiklik yapmayın.
  Vurulmuş/ölmüş düşmanı diriltmeyin; AI'yi veya global gün saatini kapatmayın.
- Hayalet modu hjs yerçekimi ve gerçek joystick CharacterController.Move yolunu
  ayrı korur. Move ABI: int(Object*, Vector3 HFA, MethodInfo*); başka kontrolcü,
  farklı oturum ve kapalı mod özgün çağrı/CollisionFlags iletir. Kapanınca normal
  hareket ve sıfırlanmış dikey hız döner. İleri/geri kamera quaternion yönünü,
  sağ/sol yatay yönü izler; durunca son yükseklik korunur. Yüksel/Alçal düğmesi
  eklemeyin. Başlangıca dönüş komutu oturumludur.
- Infinity Ammo yalnız eldeki kendi Crossbow/Shotgun atışında yük bayrağını
  çağrı boyunca hazırlar; özgün atışı bir kez çağırıp gerçek durumu korur.
  Ortak ShotgunManager bayrağını kalıcı değiştirmeyin, cooldown'u kaldırmayın.
- Klon oyuncunun kamera yönünde yatay ve dik yerleştirilir; yürünebilir zemin,
  AI bileşeni ve yeni hiyerarşinin ViewID'si doğrulanır. Geçerli ön zemin yoksa
  yana/başka yere sessizce oluşturmayın. Mevcut ana manager referansı ezilmez.
- Shotgun Ammo için yerel görsel fallback kaldırıldı. Yalnız gerçek ağ prefabı
  (PickupItem Ammo=48 + PhotonView) doğrulanırsa normal SDK ile oluşturun;
  aksi halde seçeneği kapalı gösterin. İngilizce eşya isimlerini koruyun.
- Klon AI'si MasterClient kontrolüyle çalışır; görünürlük hareket kanıtı değildir.
  Yerel master normal InstantiateRoomObject kullanır. Client kendi yeni,
  sahipliği doğrulanan klonuna yerel NavMesh devriyesi uygular. Fixed sahipliği
  veya IsMasterClient değerini değiştirmeyin; host AI/saldırısı diye sunmayın.
  Unity Start'ı elle tekrarlamayın; ana manager referansını ezmeyin.
- Granny/Grandpa geç klonunda yalnız doğrulanan sahne rota/kapı referanslarını
  field_set_value_object GC bariyeriyle yeni AI'ye bağlayın; readback uygulayın.
  View/animator/agent veya ana manager düşman yuvasını kopyalamayın. Yeni
  NavMesh rotasını SetDestination sonucu ile sınayın. Durdurma açıkken klon
  başlangıç denemesi tüketmeyin; MomSpider'ın çalışan yolunu koruyun.
- Eksik clone AI/göz yerel referanslarını hiyerarşinin çocuk sırası ve adıyla
  klonun kendi bileşenine eşleyin; source Animator/AI/agent/PhotonView paylaşmayın.
  Sadece eksik veya yanlışlıkla source'a bağlı referansları tipli GC bariyeriyle
  yazın, readback uygulayın. Statik/readonly/list/canlı hedef alanlarını kopyalamayın.
  Kendi göz-AI bağı, Animator.enabled/speed ve per-instance hedef listesi hazır
  olmalıdır. Start/OnEnable veya küresel olayları tekrar çalıştırmayın.
  Salt nav hareketini AI başarısı diye bildirmeyin; özgün AI karesi dönüşünü
  de gözleyin. Cihazda kapı/saldırı denenmeden bunu doğrulanmış sunmayın.
- Güncel dump'ta NavMeshAgent.isStopped, isOnNavMesh ve Warp YOKTUR.
  Eksik API'nin false fallback'iyle navigasyon kodunu atlamayın. speed,
  Behaviour.enabled, SetDestination, hasPath ve pathPending vardır; fixture
  tests/fixtures/navmesh_agent_dump.txt özgün metot listesini korur. Klon
  startWalk mandalını true yapmak yürüyüş girişini atlatır; false bırakın.
  Rota isteğinin kabulünü klonun yürüdüğü şeklinde göstermeyin; konum gözleyin.
- Oyuncu oylaması gerçek hosta özeldir. Süre 1–60 sn, ilk host oyu +1 evet,
  kişi/ActorNumber başına ilk oy, eşitlikte kalma ve canlı sohbet sayıları korunur.
  RPC PhotonMessageInfo boyut/hizalamasını doğrulayın; gerçek Sender üyeliği ve
  zaman damgası olmadan oy saymayın. Başlangıç duyurusu iletilmeden süre başlamaz;
  olumlu sonuç duyurusundan bir saniye önce atmayın. Normal CloseConnection
  kullanın; geçici yerel SDK kapısını her çıkışta geri yükleyin. Host taklidi,
  paket düşürme yoktur; ayrılmayı oyuncu listesinden doğrulayın. Taslağı silmeyin;
  oda/host/sahne değişiminde iptal edin. /evet ve /hayır AI'ye gönderilmez.
  CloseConnection alıcı istemcinin kabulüne bağlıdır; bu çağrının true dönmesi
  zorunlu sunucu atması değildir. Kesin atılma duyurusu yapmayın; hedef hâlâ
  listedeyse panelde ve sohbette başarısızlığı bildirin. Ayrı yetkili sunucu
  çıkarma yolu bulunmadı; paket kesme veya istemci kimliği taklidi eklemeyin.
- Boyut modsuz alıcıya iletilmediği kullanıcı cihazında gözlendi; yerel olarak
  etiketleyin. Tek tarafta stream alan sayısını artırmayın; tahmini RPC eklemeyin.
- Kullanıcı ikinci modu devre dışı bıraktığında maçın açıldığını bildirdi.
  İki modla birlikteki önceki geçersiz native atlamanın tam kaynağı belirlenmedi;
  kaldırılmış spekülatif native-call guard'ı geri eklemeyin. God Mode çalıştığı
  kullanıcı tarafından düzeltildi; son hata hayalet modundaydı.

- 22 Kasım 2026 son kullanıcı isteğiyle yerel host testleri ve Android ARM64
  derlemesi artık yetkilidir. Gerekli testleri/build'i çalıştırın ve doğrudan
  libmultiplayermod.so sağlayın. Kaynak ZIP sürümünü ve eşleşen Build ID/sembol
  adreslerini koruyun; cihazda doğrulanmayan davranışı doğrulanmış sunmayın.

- Son kullanıcı açıklaması: client'ta kendi Granny/Grandpa klonları da duruyor.
  Yeni kendi Granny/Grandpa/MomSpider klonunda sahipliği her güncellemede doğrulayan yerel
  devriye vardır. imw yalnız kendi rota listesi/difficulty kurulumu, imz kendi
  hareket animasyonu içindir; Start, imr veya global olayları tekrarlamayın.
  Client saldırı/uzak animasyon başarısını iddia etmeyin. Sahiplik kaybında
  açılan yerel agent kapatılır; gerçek master olunca özgün AI'ye dönülür.
- Klon hız/boyut değerleri yeni klona aittir; varsayılan ön mesafe 4 m,
  aralık 2–6 m. Boyut yereldir. Eski klon veya asıl düşmana taşımayın.
  Granny/Grandpa yürüyüş düzeltmesi gerçek nav velocity + tipli statik hash
  okuması kullanır; saldırı/kapı/ölüm/durdurma sırasında animasyonu ezmeyin.
- Seçili AI kişilerinde isim kapısı uygulanmaz; doğrudan başka kişiye hitap
  filtresi kalır. İsim hatırlatmasını yalnız Herkes + isim filtresi için yapın.

- Son client God Mode regresyonu: HandleEnemyAttacked RPC_GetBatted hedef
  eşleştirmesinden önce ölüm olayını yayınlar; sk.MoveNext gecikmiş fatal olay
  çalıştırır. Bu RPC'yi düşman animasyonu adına açmayın. Kendi hedefi istek ve
  RPC sınırında engellenir; enemy FixedUpdate/inh saldırı animasyonu korunur.
  Uzak modsuz hostun gün/ölüm kararını yalnız client'ta iptal garantisi vermeyin.
- Oda adı kozmetik Text() filtresiyle okunmaz: özgün managed string kökü
  saklanır, JoinRoom'a aynı nesne iletilir. Uzun/emoji/etiketli adı kesmeyin,
  UTF-8'den yeniden üretmeyin. Bitiş/timeout'ta kökü bırakın; başarısız
  LeaveRoom bool sonucunda yeniden katılma akışı başlatmayın.
- Hosttan seçici paket kesme Photon sohbet erişimini kapatmaz; sohbet relay'i
  ayrı Photon hizmetindedir. Normal CloseConnection ve gerçek ayrılma
  doğrulaması korunur, işlevsiz paket düşürme ile atma başarısı sunulmaz.

- Son filtre isteği: Mesaj filtresi kapalıyken kahkaha/karalama/emoji ve diğer
  uygun mesajlar AI'ye gider. Selamlar HER ZAMAN sabit yerel cevaplanır. Model
  sistem talimatındaki anlamsızsa SKIP kuralını da düğmeyle birlikte değiştirin.
  Hız beklemelerini değiştirmeyin; ada göre süz ve kişi seçimi ayrı kalır.
- Gerçek kick: setupPlayerRoomItem.fgu(Player) satır eşlemesini normal çağrıdan
  sonra yakalar; aynı gerçek Room/Player için etkin satır varsa fgv(Player)
  kullanılır. İsimle veya sahte UI/Player ile eşleme yapmayın. fgv dönüşü void
  ve kendisi de CloseConnection çağırıp UI satırını yok eder: bunu atılma kanıtı
  saymayın. Oda/host/hedef kontrolü ve ayrılma doğrulaması korunur.
- Host klonunda göz hazırlığı başarısızlığı yürüyüşü engellememeli. Nav/animator
  yalnız yeni sahipli hiyerarşiden tipli bağlanır; updatePosition/updateRotation
  setter'ları dump'ta vardır, getter'ları yoktur. Durdurma/gerçek ölüm korunur.
- Son kullanıcı isteği: Rat/Spider klonlama düğmelerini ve oluşturma yollarını
  kaldırın; yalnız Granny/Grandpa/MomSpider klonlanır. Mevcut Rat/Spider
  durdurma/ayarları korunur. Durdurma ayrı ana sayfa değil Düşman öldür içindeki
  Ayarlar sekmesindedir. Oylama paneli kullanıcının son düzeltmesiyle KALIR.
- Sınırlı clone probe logu nav/rota/konum/AI kare durumunu taşır; isim, oda adı,
  sohbet ve anahtar yazmayın. Salt hedef kabulü/hareket tam AI başarısı değildir.

- Son cihaz gözlemi: klon boyutu karşı cihazda görünüyor. Önceki genel 'boyut
  yalnız yerel' çıkarımını bütün prefablara uygulamayın; mevcut akışı bozmayın.
- Klon oyuncu listesi yeterli algı değildir: gerçek yerel masterda, yeni sahipli
  klonun kendi EnemyEye/EnemyEyeGrandpa.CheckForPlayersInView metodu 0,25 sn,
  GetPlayers listesi 2 sn aralıkla çalıştırılır. MomSpider ims/inc kullanır.
  Gözün PV'si kaynağın değil yeni klonunki olmalı; boş görüş menzili/parse oranı
  typed Single olarak kaynaktan alınır. LayerMask boyutu doğrulanmadan yazılmaz.
  Orijinal hedef/atak mantığını, gerçek ölümü ve durdurmayı koruyun; master
  taklidi veya zorla ölüm RPC'si eklemeyin. Host cihaz doğrulaması beklenir.
- KickTrace SDK CloseConnection dönüşünü yalnız kendi çağrısında gerçek GC
  hedefiyle eşler, her çıkışta önceki ölçüm bağlamını geri yükler. Native fgv'nin
  UI satırını silmesini başarı saymayın. Modsuz alıcının SDK OnEvent kodundaki
  EnableCloseConnection kapısını hostta bool yazarak değiştiremezsiniz.

- Yeni dönüş sürgüsü gerçek hostta GrannyManager.eoo / GrandpaManager.dzn
  hesabını 0–150 sn ile sınırlar; sonraki normal ölümde geçer. Ortak manager
  süresini tek klona özel geri dönüş diye sunmayın; RemoveEnemy diriltme değildir.
- Hamarat God Mode'a bağlıdır. Yalnız kendi köklü inventory.hqw çağrısında,
  bakılan aynı WorldInteractable için gerçek requiredItems listesinden geçici
  PickUpObject seçilir. Elde görsel eşya üretmeyin, gerçek eşyayı tüketmeyin.
  Native ön koşul/mesafe/cooldown ve fgd → normal olay/RPC akışını koruyun.
  Taze native hand değişikliğini geri yüklemeyle ezmeyin; scope her çıkışta biter.
- WorldInteractable.fge/fgj yalnız iki komut olabilir: DobbyHookNear ile tam
  dört bayt değiştirin. Uzak uzun trampoline fallback yoktur; allocation,
  relocation veya patch başarısızsa özelliği kapalı tutun. Dobby genel Hook
  yolunu değiştirmeyin. NearBranch build bağımlılığı açıktır.
- Zorluk GameDifficulty'nin byte ABI'si ve statik Difficulty alanı doğrulanınca
  gerçek hostta değişir. eik/dum normalde dövüşü ve rotayı sıfırlar: yalnız
  köklü yenileme çağrısında eim/eil/ein ve duo/dun/dup yardımcılarını atlayın.
  Başka AI'nin veya normal Start'ın yardımcılarını engellemeyin. Mom imw için
  yalnız geçici rota listesi bağlayıp her çıkışta asıl listeyi geri verin.
  Preset/ödül/uzak menü etiketi eşitliği garantisi vermeyin. Metadata adresi/
  alan offset'i sabitlemeyin, host taklidi veya özel ağ paketi eklemeyin.
