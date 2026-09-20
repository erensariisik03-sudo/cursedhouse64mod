# CursedHouseMod — ARM32 + ARM64 chat limit kaynakları

Mevcut çalışan ARM32 kaynak korunmuştur. ARM64 (`arm64-v8a`) için ayrı kaynak ve AArch64 inline-hook katmanı eklenmiştir.

## Doğrulanan ARM64 değerleri

Kaynakta kullanılan değerler, proje içindeki 64-bit `dump.cs` ve `veri.txt` ile eşleştirilmiştir:

### Oyun chat sınıfı
- `chat.inputField` = `+0x30`
- `chat.Update` = `0x1F22B2C`

### TMP_InputField
- `m_CharacterLimit` = `+0x19C`
- `get_characterLimit` = `0x3F3A6BC`
- `set_characterLimit` = `0x3F3A6C4`
- `Append(string)` = `0x3F43124`
- `Append(char)` = `0x3F431D4`
- `Insert(char)` = `0x3F43548`
- `ActivateInputFieldInternal` = `0x3F3E0AC`
- `UpdateTouchKeyboardFromEditChanges` = `0x3F40C18`

### UIElements fallback
- `ITextEdition.get_maxLength` = `0x41A75C8`
- `ITextEdition.set_maxLength` = `0x41A75D0`
- `get_maxLength` = `0x41A76F8`

### TouchScreenKeyboard fallback
- `set_characterLimit` = `0x3FEFCC0`
- `set_characterLimit_Injected` = `0x3FEFD14`

## ARM64 adres kuralı

`runtimeAddress = libil2cppLoadBias + RVA`

ARM32'deki Thumb `+1` kullanılmaz. `-0x10000` düzeltmesi de kullanılmaz.

## Kaynak yapısı

ARM32:
- `src/main.cpp`
- `src/arm_hook.cpp`

ARM64:
- `src/main64.cpp`
- `src/arm64_hook.cpp`
- `include/arm64_hook.h`

CMake, `ANDROID_ABI` değerine göre doğru kaynakları otomatik seçer.

## Build

```bash
./build.sh armeabi-v7a
./build.sh arm64-v8a
```

GitHub Actions iki ABI'yi de ayrı artifact olarak derler:
- `multiplayermod-armeabi-v7a`
- `multiplayermod-arm64-v8a`

## Önemli ARM64 hook notu

AArch64 hook katmanı hedef fonksiyonun ilk 16 byte'ını trampoline'e kopyalar. Normal IL2CPP fonksiyon prologlarında bu genellikle STP/MOV/SUB/LDR/STR gibi doğrudan talimatlardan oluşur. İlk 16 byte içinde PC-relative bir talimat bulunursa onun ayrıca relocate edilmesi gerekir; böyle bir hedefte logcat'teki ilgili hook adresi kontrol edilmelidir.
