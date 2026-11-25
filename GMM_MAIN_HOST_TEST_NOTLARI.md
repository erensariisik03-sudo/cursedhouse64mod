# Main.cpp host testi ve oda limiti

Temel: `GMM_AI_TekMesaj_YerelHost_TAM_KAYNAK.zip`. Yalnızca yerel host arayüzü ve oda limiti davranışı güncellendi.

- Main(1).cpp host parçasındaki `PhotonNetwork.get_IsMasterClient` yerel true hook'u zaten vardı, korundu.
- Eski dosyadaki `launcher.Update -> toggleMasterClientOptionsWindow(true)` çağrısı yalnızca **Host Modu açılınca bir defa**, IL2CPP metadata yöntemi bulunursa çalıştırılır. Eski RVA kullanılmaz.
- Oyuncu limiti 1–255; gerçek Photon `Room.MaxPlayers` setter kullanılır, okuma ile doğrulanır. Yerel Host Modu sırasında da gönderim denenebilir; başarı **garanti edilmez**.
- Eski Main(1).cpp'deki `customMaxPlayers` yalnızca kullanıcı girdisine atanıyordu; sunucuya uygulayan kod yoktu. Bu yüzden taşınmadı.
- `RemovePlayer`, `OnMasterClientSwitched` engellemesi, bellek offseti 0x48/0x1E/0x24, anti-kick ve diğer Main.cpp hook'ları **taşınmadı**. Gerçek host sunucuda değişmezse iki-host yan etkileri sürebilir.

Test logları: `adb logcat -d -s multiplayermod:I | Select-String "GMM local host|GMM room admin"`. `launcher_ui=0` çıkarsa oyundaki metot bulunamadığından çağrı devreye girmez. `set_limit accepted=1` yalnızca çağrı sonucu, gerçek uygulama için menüdeki `max_players` geri okumasını kontrol et.
