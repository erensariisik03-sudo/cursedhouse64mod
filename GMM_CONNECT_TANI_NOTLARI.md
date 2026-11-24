# GMM Connect — karışık fotoğraf menüsü ve Photon tanılama düzeltmesi

## Değişiklikler
- Geliştirici ekranındaki eski yerel fotoğraf listesi ve `Seçilen fotoğrafı teklif et` düğmesi kaldırıldı. Fotoğraf istemek için `Genel AI > GMM Connect` kullanın. Geliştiriciden bu sayfaya gitme kısayolu eklendi.
- GMM Connect'e Photon kanalı hazır değilse açık uyarı eklendi; oyuncu satırının seçilmesi gerektiği belirtildi.
- `Oyuncu listesini yenile` artık gerçekten Photon odasındaki oyuncuları anında yeniden tarıyor (eskiden sadece yerel resim klasörünü tarıyordu).
- EventData `Code` ve `Sender` için, oyunun Photon derlemesindeki alan/metot farklarına uyum amaçlı alternatif okuma eklendi.
- Ağ bağlantısı kurulamazsa IL2CPP/Photon çözümleme adımlarını yazan tanı logları eklendi. Kişisel fotoğraflar veya API anahtarları loglanmaz.
- Önceden mevcut olan izinli paylaşım modeli korunuyor: karşı taraf kabul etmeden fotoğraf listesi gönderilmiyor, sadece Pictures/GMMShare klasörüne izin var.

## Test
- İlgili ImGui menü + regresyon testi: PASS
- Photon browse ve fotoğraf parçası/kontrol testleri: PASS
- Android uyumlu C++ sözdizimi: PASS (gerçek NDK derlemesi ve iki cihaz aktarımı henüz test edilmedi)

## Cihaz testinde
1. Aynı yeni ARM64 `.so` dosyasını iki telefona kur ve aynı Photon odasına gir.
2. Gönderen/sahip cihazda `/sdcard/Pictures/GMMShare` içine 256 KB'den küçük bir JPG veya PNG yerleştir.
3. İsteyen cihazda `Genel AI > GMM Connect` açıp hedef oyuncu satırına dokun ve `Hedef oyuncudan fotoğrafları iste` de.
4. Sahip cihazda görünen izin isteğini kabul et. Kabulden önce dosya listesi görünmez.
5. Liste gelmezse her iki cihazın Logcat kayıtlarını topla:
   `adb logcat -d -s multiplayermod:I | Select-String "GMM photo|Photon"`
   Kullanıcı adları, cihaz bilgileri veya diğer kişisel satırlar bulunuyorsa göndermeden önce çıkar.

ÖNEMLİ: Bu paket bir **tanı + arayüz düzeltmesi**dir, iki gerçek cihazda Photon taşımasının çalıştığını henüz garanti etmez. DEX'i değiştirmeye gerek yok.
