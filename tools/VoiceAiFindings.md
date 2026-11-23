# Sesli AI — uygulama ve doğrulama

## Araştırma

- Photon Voice PUN entegrasyonu, ayrı Voice odasının oyun odasını izlemesini
  sağlar. Duyabilmek için alıcı da aynı Voice odasında olmalıdır:
  https://doc.photonengine.com/voice/v2/getting-started/voice-for-pun
- Recorder AudioClip kaynağı ve kayıt/yayın özellikleri:
  https://doc-api.photonengine.com/en/voice/current/class_photon_1_1_voice_1_1_unity_1_1_recorder.html
- Gemini ses girişi:
  https://ai.google.dev/gemini-api/docs/audio
- Android TextToSpeech (synthesizeToFile): https://developer.android.com/reference/android/speech/tts/TextToSpeech

Bu inceleme 23 Kasım 2026 tarihinde erişilen belgeler ile sağlanan ARM64
libil2cpp.so, global-metadata.dat ve dump.cs üzerinden yapıldı. API erişimi ve
kotalar gerçek anahtarla denenmedi. Belgelerdeki TTS isteği HTTP fixture'ı ile
karşılaştırıldı; gerçek servis yanıtı gözlemlenmedi.

## Bağlantı ve yetki

VoiceChatManager.gqc yalnız kullanılabilirliktir, Voice odasına katılma kanıtı
değildir. Oda CustomProperties["VoiceChatEnabled"] true ve Voice Client.InRoom
birlikte aranır. Hashtable System.Object overload'u metadata ile seçilir.
Kapalı odada yerel AudioSource cevapları okur. Başka modsuz oyuncuların bu
odada sesi duyması sağlanmış sayılmaz; odanın host tercihi değiştirilmez.
Host olmayan istemci de kendi mevcut ses bağlantısını kullanabilir.

## Ses işleme

RemoteVoiceLink.OnDecodedFrameFloatAction gerçek FrameOut<float> sınıfıyla
hook'lanır. Orijinal playback daima iletilir. Player kimliği PhotonVoiceView
PhotonView.Owner üzerinden alınır. Pinned float dizisinin başlangıcı IL2CPP
array_object_header_size export'undan çözülür; tahmini başlık/offset kullanılmaz.
Audio thread yalnız sınırlı PCM kopyalar; HTTP/Unity nesne işlemi yapmaz.
16 kHz mono WAV, konuşma/sessizlik bölümlendirmesi, Gemini transkripsiyon,
mevcut sohbet model havuzu, ardından cihazın yerel Android TextToSpeech motoru (tr-TR,
düşük perde; Gemini TTS kaldırıldı, ağ/kota yok). Ses yanıtı en çok 45 saniye PCM16 mono kabul edilir.
Yankı metni, kendi mesajlarımız ve /evet-/hayır komutları tekrar işlenmez.

## Mikrofon ayrımı

Önce Recorder kayıt/yayın durdurulur ve kayıt kapalı okunur. Ağ için AudioClip
kaynağına geçilir, klip okunarak doğrulanır; Loop/VAD kapatılır. Ek etkin
Recorder mikrofonları da durdurulur. set_RecordingEnabled ve StartRecording
hook'ları yeni mikrofon açılışını engeller. Klip tekrar başlatılıp yayın açılır;
cihazda ayrı AudioSource ile de çalınır. Aynı oda/nesnede güncel kullanıcı
mikrofon tercihi geri yüklenir. Eski oda mikrofonu açılmaz. Kurulum başarısızsa
ses oynatılmaz, metin yanıtına geçilir. Donanım/oyun sürümü doğrulaması gerekir.

## Regresyon ve cihaz kontrolü

Saf çekirdek: WAV/base64/bozuk format, VAD, örnekleme, üç giriş/çıkış akışı,
TTS hatası, API anahtarı izolasyonu, isim/yankı filtresi ve aşamalar arası iptal.
Native fixture: birincil/ek mikrofon durdurma, yeniden açma engeli, son tercihi
geri yükleme, geçersiz odada mikrofonu kapalı tutma, gerçek frame hook'unda PCM
kopyası ve orijinal forwarding. Sohbet fixture'ı yazılı bot kapalıyken sesli
AI metin yönlendirmesini, kendi satırları ve oy komutlarının atlanmasını sınar.
ImGui beş çözünürlükte sayfa geçişi/mod/düğme ve dokunma önceliğini sınar.

Cihazda: iki farklı oyuncu, Herkes/isim şartı/seçili kişi; üç mod; voice açık ve
kapalı oda; AI konuşurken mikrofona konuşma; ses ortasında Durdur/çıkış/host
ses ayarı; API kota/ağ kesilmesi ve oynatmadan sonra mikrofon geri dönüşü.
Düşman görme testi God kapalı yapılmalı. Geç bağlanan klon ilk 12 denemeden
sonra da 10 saniye arayla bağlamayı dener. Native AI/host yetkisi korunur.
