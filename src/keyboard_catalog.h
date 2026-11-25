#pragma once
#include <cstddef>
#include <cstdint>
namespace keyboard_catalog {
struct Color { const char* name; const char* rgba; };
inline constexpr Color kColors[] = {
    {u8"Kırmızı", "F00F"}, {"Turuncu", "F80F"}, {u8"Sarı", "FF0F"},
    {u8"Yeşil", "0F0F"}, {"Turkuaz", "0FFF"}, {"Mavi", "00FF"},
    {"Mor", "80FF"}, {"Pembe", "F0FF"}, {"Beyaz", "FFFF"}, {"Siyah", "000F"}
};
struct Emoji { uint32_t codepoint; const char* utf8; const char* name; };
// Candidates only: the game font/sprite probe decides which are actually offered.
inline constexpr Emoji kEmojis[] = {
    {0x263A, u8"☺", u8"Gülümse"}, {0x263B,u8"☻",u8"Gülen yüz"},
    {0x2665,u8"♥","Kalp"}, {0x2764,u8"❤",u8"Sevgi"},
    {0x2605,u8"★",u8"Yıldız"}, {0x2606,u8"☆",u8"Boş yıldız"},
    {0x2713,u8"✓","Onay"}, {0x2714,u8"✔",u8"Kalın onay"},
    {0x2716,u8"✖",u8"Çarpı"}, {0x2600,u8"☀",u8"Güneş"},
    {0x2601,u8"☁","Bulut"}, {0x2602,u8"☂",u8"Şemsiye"},
    {0x2603,u8"☃",u8"Kardan adam"}, {0x2615,u8"☕","Kahve"},
    {0x2639,u8"☹",u8"Üzgün"}, {0x2620,u8"☠","Tehlike"},
    {0x26A0,u8"⚠",u8"Uyarı"}, {0x26A1,u8"⚡",u8"Şimşek"},
    {0x2708,u8"✈",u8"Uçak"}, {0x2709,u8"✉","Mektup"},
    {0x270C,u8"✌",u8"Barış"}, {0x2728,u8"✨",u8"Parıltı"},
    {0x2744,u8"❄","Kar"}, {0x274C,u8"❌","Hayir"},
    {0x2753,u8"❓","Soru"}, {0x2757,u8"❗",u8"Ünlem"},
    {0x1F600,u8"😀",u8"Mutlu"}, {0x1F602,u8"😂",u8"Kahkaha"},
    {0x1F60D,u8"😍",u8"Aşık"}, {0x1F609,u8"😉",u8"Göz kırp"},
    {0x1F60E,u8"😎",u8"Havalı"}, {0x1F622,u8"😢",u8"Ağla"},
    {0x1F62D,u8"😭",u8"Gözyaşı"}, {0x1F621,u8"😡",u8"Kızgın"},
    {0x1F44D,u8"👍",u8"Beğen"}, {0x1F44E,u8"👎",u8"Beğenme"},
    {0x1F44F,u8"👏",u8"Alkış"}, {0x1F64F,u8"🙏",u8"Teşekkür"},
    {0x1F4AA,u8"💪",u8"Güç"}, {0x1F525,u8"🔥",u8"Ateş"},
    {0x1F389,u8"🎉",u8"Kutlama"}, {0x1F680,u8"🚀",u8"Roket"},
    {0x1F3C6,u8"🏆",u8"Kupa"}, {0x1F480,u8"💀",u8"Kafatası"},
    {0x1F47B,u8"👻",u8"Hayalet"}
};
inline constexpr size_t kEmojiCount = sizeof(kEmojis) / sizeof(kEmojis[0]);
static_assert(kEmojiCount <= 64);
inline constexpr uint64_t kAllEmojiMask = (uint64_t{1} << kEmojiCount) - 1;
}
