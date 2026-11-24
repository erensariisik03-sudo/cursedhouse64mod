# GMM Connect — Photon Adım Adım Tanı (24.11.2026)

Bu paket sorunun nedenini tanımlamak için ağ aşamalarına tanı logları ekler.
`GMM photo transport hook=1`, Photon `OnEvent` kancasının kurulmuş olduğunu gösterir; oda, hedef oyuncu ve paket iletimini **tek başına doğrulamaz**.

Her iki cihaza da aynı `.so` sürümünü kurup aynı Photon odasına girin.

İsteyen cihazda `Genel AI > GMM Connect` bölümünün üstündeki `Tani: hook=... | oda=... | aktor=... | oyuncu=... | secilen=... | istek=...` satırına bakın.

- `hook=1 oda=0`: Photon sınıfları mevcut ama bu kod `CurrentRoom` görmüyor.
- `oda=1 oyuncu=0`: Oda var, `PlayerListOthers` boş; hedef seçilemez.
- `secilen=0`: Bir oyuncu satırına dokunulmamış.
- `istek=1`: Önceki fotoğraf listesi talebi sürüyor. **Fotoğraf listesi isteğini iptal et** ile tekrar deneyebilirsin.

Windows PowerShell'de:

```powershell
adb logcat -c
# Oyuna gir, iki cihazı aynı odaya sok; GMM Connect'te oyuncuyu seçip isteğe bas.
adb logcat -d -v time -s multiplayermod:I | Select-String "GMM photo"
```

`GMM photo TX: protocol=2 kind=R ... ok=1`: fotoğraf listesi isteğinin Photon istemcisine verildiği anlamına gelir.
`GMM photo RX: protocol=2 kind=R`: diğer cihaz bu isteği almıştır.
İstek sahibi diğer cihazda `Kabul et` dediğinde `TX ... kind=G` ve isteyen tarafta `RX ... kind=G` görülmelidir.
`kind=L` fotoğraf listesi satırı, `kind=E` liste sonu, `kind=F` dosya isteğidir.
Loglar fotoğraf bytes, dosya adı veya parola içermez.

NOT: Bu bir tanı yamadır; gerçek iki cihaz iletişimi burada fiziksel olarak test edilemez.
