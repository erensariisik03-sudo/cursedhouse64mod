# Klon ve ağ davranışı — güncel sürüm

Referans: kullanıcının ARM64 libil2cpp.so, dump.cs ve global-metadata.dat dosyaları.
RVA/offset değerleri inceleme kanıtıdır; çalışma anında metadata çözülür.

| Konu | Güncel davranış |
| --- | --- |
| Asıllar kaldırılınca kurulum | Asıl düşman zorunluluğu kaldırıldı; klonun kendi bileşenleri ve tekil sahne DaysManager kullanılır. |
| Görme bağlantısı | AI ve Eye aynı klonun PhotonView'ını alır; kamera hiyerarşi içinde doğrulanır. Grandpa'da daysManager bulunmadığından istenmez; grandpaUpdateParseRate kullanılır. |
| Takip/saldırı | Gerçek hostun native CheckForPlayersInView / Mom inc akışı korunur. Kısa görüş kesilmesi için 2 saniye pay; ardından örneğe özel native hedef temizleme ve devriye. |
| Kapılar | Öndeki ilk collider'ın Door üst nesnesi; yalnız kapalı/hareketsiz/kilitsiz kapıda normal ewl etkileşimi. Türler arası ortak 2 saniye bekleme. |
| Ses | Native ses callback’leri ve oyuncu adım olayları; adımlar 8 m, nesne sesleri 20 m, 10 saniyeye kadar araştırma ve varışta 2 saniye bekleme. Görüş/saldırı öncelikli; ortak waypoint değiştirilmez. |
| Takılma | Sağlıklı sahipli klon: 12 s hareketsizlik / 15 s takılı saldırı sonrası native devriye toparlaması; durdurma ve yaralanma korunur. |
| Client klon | Kendi klonunun yerel devriye ve kapı köprüsü korunur. Tam native saldırı hosta aittir; master taklidi ve ikinci saldırı otoritesi yoktur. |
| Şaka oylaması | Host/client başlatabilir; gerçek Player kimliğiyle /evet–/hayır. Evet çoğunluğu yalnız iki aşamalı sohbet şakası gönderir. Gerçek atma UI/SDK hook ve çağrıları kaldırıldı. |
| Durdur | Seçilen kategorinin asılları ve klonları. Client'ta sonuç yereldir. |

`clone probe` ve `GMM_DIAG clone` hareket, görüş, kapı ve ses durumlarını gösterir.
Bir native RPC çağrısının dönmesi, karşı cihazda uygulandığına dair onay değildir.
Cihazda kapı geometrisi, gürültü prefabları ve gerçek saldırı sırası ayrıca denenmelidir.
