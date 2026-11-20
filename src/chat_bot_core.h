#pragma once
// Saf (oyundan bağımsız) sohbet botu mantığı: etiket temizleme, gönderen eşleme,
// cevap temizleme ve Gemini JSON. Host testleri tests/chat_bot_test.cpp içindedir.
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace chat_bot_core {
struct Turn { bool model = false; std::string text; };

inline bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
inline std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && IsSpace(s[a])) ++a;
    while (b > a && IsSpace(s[b - 1])) --b;
    return s.substr(a, b - a);
}
// "<color=#fff>Ali</color>: merhaba" -> "Ali: merhaba". '<' ile '>' arasını siler;
// kapanmamış '<' olduğu gibi kalır.
inline std::string StripTags(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '<') {
            const size_t close = in.find('>', i);
            if (close != std::string::npos && close - i <= 64) { i = close; continue; }
        }
        out += in[i];
    }
    return out;
}
inline size_t ScalarCount(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}
inline std::string PrefixScalars(const std::string& s, size_t max) {
    size_t n = 0, i = 0;
    for (; i < s.size(); ++i) {
        if (((unsigned char)s[i] & 0xC0) != 0x80 && ++n > max) break;
    }
    return s.substr(0, i);
}
inline bool IsWordChar(unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0x80 || c == '_'; }
inline bool IsSep(char c) { return c == ':' || c == ']' || c == ')' || c == '>' || c == '-' || IsSpace(c); }

// Satır "Ad: mesaj" biçiminde değilse nullopt. Ad, satırın başında (en çok 12 bayt
// önek, örn. "[" veya saat) bulunmalı ve ardından ayırıcı gelmelidir.
inline std::optional<std::string> MatchSender(const std::string& line, const std::string& name_in) {
    const std::string plain = Trim(StripTags(line));
    const std::string name = Trim(StripTags(name_in));
    if (name.empty() || plain.size() <= name.size()) return std::nullopt;
    const size_t limit = std::min<size_t>(12, plain.size() - name.size());
    for (size_t pos = 0; pos <= limit; ++pos) {
        if (plain.compare(pos, name.size(), name) != 0) continue;
        bool prefix_ok = true; // ad satır başında olmalı: önünde harf/rakam olamaz ("Veli: Ali: x" reddedilir)
        for (size_t k = 0; k < pos; ++k) if (IsWordChar((unsigned char)plain[k])) prefix_ok = false;
        if (!prefix_ok) continue;
        size_t end = pos + name.size();
        if (end >= plain.size() || !IsSep(plain[end])) continue;
        bool real_sep = false;
        while (end < plain.size() && IsSep(plain[end])) { real_sep = real_sep || !IsSpace(plain[end]); ++end; }
        if (!real_sep) continue; // "Ali nasılsın" gibi ad+boşluk kabul edilmez
        std::string rest = Trim(plain.substr(end));
        if (rest.empty()) return std::nullopt;
        return rest;
    }
    return std::nullopt;
}
// Yapay zekâ çıktısı sohbete gitmeden önce: etiket karakterleri, satır sonları ve
// kontrol karakterleri atılır, uzunluk sınırlanır.
inline std::string CleanReply(const std::string& in, size_t max_scalars) {
    std::string out;
    bool space = false;
    for (unsigned char c : in) {
        if (c == '<' || c == '>' || c == '*' || c == '`' || c == '#') continue;
        if (c < 0x20 || c == 0x7F) { c = ' '; }
        if (c == ' ') { if (!space && !out.empty()) out += ' '; space = true; continue; }
        space = false; out += static_cast<char>(c);
    }
    return Trim(PrefixScalars(Trim(out), max_scalars));
}

// Uzun cevabı sohbet satırlarına böler: önce cümle sonu (. ! ?), yoksa boşluk, o da yoksa
// sert kesim; çok baytlı karakter ortadan kesilmez. En çok max_chunks parça döner.
inline std::vector<std::string> SplitChunks(const std::string& text, size_t max_scalars, size_t max_chunks) {
    std::vector<std::string> out;
    std::string rest = Trim(text);
    while (!rest.empty() && out.size() < max_chunks) {
        if (ScalarCount(rest) <= max_scalars) { out.push_back(rest); break; }
        const std::string head = PrefixScalars(rest, max_scalars);
        size_t cut = std::string::npos;
        for (size_t i = head.size(); i-- > head.size() / 3;)
            if ((head[i] == '.' || head[i] == '!' || head[i] == '?') && (i + 1 == head.size() || head[i + 1] == ' ')) { cut = i + 1; break; }
        if (cut == std::string::npos) {
            const size_t sp = head.rfind(' ');
            cut = (sp != std::string::npos && sp > head.size() / 3) ? sp : head.size();
        }
        const std::string piece = Trim(rest.substr(0, cut));
        if (!piece.empty()) out.push_back(piece);
        rest = Trim(rest.substr(cut));
    }
    return out;
}
inline std::string JsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break; case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break; case '\r': o += "\\r"; break; case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += static_cast<char>(c);
        }
    }
    return o;
}
inline void AppendUtf8(std::string& o, uint32_t cp) {
    if (cp < 0x80) o += static_cast<char>(cp);
    else if (cp < 0x800) { o += static_cast<char>(0xC0 | (cp >> 6)); o += static_cast<char>(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { o += static_cast<char>(0xE0 | (cp >> 12)); o += static_cast<char>(0x80 | ((cp >> 6) & 63)); o += static_cast<char>(0x80 | (cp & 63)); }
    else { o += static_cast<char>(0xF0 | (cp >> 18)); o += static_cast<char>(0x80 | ((cp >> 12) & 63)); o += static_cast<char>(0x80 | ((cp >> 6) & 63)); o += static_cast<char>(0x80 | (cp & 63)); }
}
inline bool Hex4(const std::string& s, size_t i, uint32_t& v) {
    if (i + 4 > s.size()) return false;
    v = 0;
    for (size_t k = 0; k < 4; ++k) {
        const char c = s[i + k]; v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0'; else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10; else return false;
    }
    return true;
}
// "key": "..." biçimindeki ilk dize değerini çözer (\uXXXX ve vekil çiftler dahil).
inline std::optional<std::string> JsonStringAfterKey(const std::string& json, const std::string& key, size_t from = 0) {
    const std::string needle = "\"" + key + "\"";
    for (size_t at = json.find(needle, from); at != std::string::npos; at = json.find(needle, at + 1)) {
        size_t i = at + needle.size();
        while (i < json.size() && IsSpace(json[i])) ++i;
        if (i >= json.size() || json[i] != ':') continue;
        ++i;
        while (i < json.size() && IsSpace(json[i])) ++i;
        if (i >= json.size() || json[i] != '"') continue;
        ++i;
        std::string out;
        while (i < json.size() && json[i] != '"') {
            if (json[i] != '\\') { out += json[i++]; continue; }
            if (++i >= json.size()) return std::nullopt;
            const char e = json[i++];
            switch (e) {
                case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                case 'b': case 'f': out += ' '; break;
                case 'u': {
                    uint32_t cp = 0; if (!Hex4(json, i, cp)) return std::nullopt; i += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= json.size() && json[i] == '\\' && json[i + 1] == 'u') {
                        uint32_t lo = 0;
                        if (Hex4(json, i + 2, lo) && lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); i += 6; }
                    }
                    if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
                    AppendUtf8(out, cp); break;
                }
                default: out += e;
            }
        }
        if (i >= json.size()) return std::nullopt;
        return out;
    }
    return std::nullopt;
}
// candidates[0].content.parts[*].text -- Gemini cevabında ilk "text" alanı.
inline std::optional<std::string> ExtractText(const std::string& json) {
    const size_t cand = json.find("\"candidates\"");
    if (cand == std::string::npos) return std::nullopt;
    return JsonStringAfterKey(json, "text", cand);
}
inline void TrimHistory(std::vector<Turn>& h, size_t max_turns) {
    if (h.size() > max_turns) h.erase(h.begin(), h.begin() + (h.size() - max_turns));
    while (!h.empty() && h.front().model) h.erase(h.begin());
}
inline bool ModelWantsZeroThinking(const std::string& model) { return model.find("2.5") != std::string::npos; }
// Ardışık aynı rol birleştirilir; Gemini sıralı user/model ister.
inline std::string BuildRequest(const std::string& system, const std::vector<Turn>& history, const std::string& model, unsigned max_tokens = 900) {
    std::string o = "{";
    if (!system.empty()) o += "\"systemInstruction\":{\"parts\":[{\"text\":\"" + JsonEscape(system) + "\"}]},";
    o += "\"contents\":[";
    bool first = true; std::string pending; bool pending_model = false, have = false;
    auto flush = [&] {
        if (!have) return;
        if (!first) o += ",";
        o += std::string("{\"role\":\"") + (pending_model ? "model" : "user") + "\",\"parts\":[{\"text\":\"" + JsonEscape(pending) + "\"}]}";
        first = false; have = false; pending.clear();
    };
    for (const auto& t : history) {
        if (have && t.model == pending_model) { pending += "\n" + t.text; continue; }
        flush(); pending = t.text; pending_model = t.model; have = true;
    }
    flush();
    o += "],\"generationConfig\":{\"maxOutputTokens\":" + std::to_string(max_tokens) + ",\"temperature\":0.9";
    if (ModelWantsZeroThinking(model)) o += ",\"thinkingConfig\":{\"thinkingBudget\":0}";
    o += "}}";
    return o;
}
// ---- Kişilik şablonları ve çıktı sınırı -------------------------------------------------
inline constexpr size_t kMaxReplyWords = 45; // varsayılan ve en alt sınır (kullanıcı sürgüyle artırabilir)
inline constexpr int kMinWords = 45, kMaxWords = 150;
inline int ClampWords(int w) { return w < kMinWords ? kMinWords : (w > kMaxWords ? kMaxWords : w); }
inline constexpr int kPersonaCount = 3; // 0 normal, 1 anı anlatan, 2 imam; 3 = kullanıcının kendi metni
inline const char* PersonaName(int mode) {
    switch (mode) { case 0: return u8"Normal"; case 1: return u8"Anı"; case 2: return u8"İmam"; default: return u8"Özel"; }
}
inline const char* PersonaBody(int mode) {
    switch (mode) {
        case 1: return u8"Karakterin: eskiden yaşadıklarını anlatmayı seven, nostaljik bir oyuncu. Soru sorulursa önce soruyu cevapla, "
                       u8"sonra 'bir zamanlar şöyle olmuştu, sonra böyle oldu' tarzında çok kısa bir anı ekle.";
        case 2: return u8"Karakterin: sakin, nazik ve bilge bir imam gibi konuşan oyuncu. Biri soru sorduğunda ÖNCE soruyu gerçekten ve doğru cevapla, "
                       u8"sonra üslubunla ('evladım', 'hayırlı olsun') bir iki cümlelik öğüt ya da söz ekle; asla soruyu geçiştirip yalnız dua ya da selamla yetinme. "
                       u8"Her soruyu cevaplamaya çalış; 'bunu bilen birine sor' demek yalnızca hiçbir şekilde cevap veremediğin en çıkmaz durumda son çare olsun. "
                       u8"Dini konularda bilinen genel bilgiyi ve yaygın görüşü anlat, kesin fetva verme; kimseyle alay etme. Selam ve sohbet cümlelerine kısa karşılık ver.";
        default: return u8"Karakterin: normal, samimi ve yardımsever bir yapay zekâ. Soruya doğrudan cevap ver.";
    }
}
// Tüm kişiliklerin ortak çerçevesi: oyun sohbeti, birden çok kişi, uzunluk kuralı, tam cümle.
inline std::string BuildSystem(int mode, const std::string& custom, size_t max_words = kMaxReplyWords) {
    const bool compact = max_words <= 60;
    std::string s;
    s += u8"Bir oyunun oda sohbetindesin ve odadaki oyunculara yapay zekâ olarak cevap veriyorsun. ";
    s += u8"Birden fazla oyuncu sana yazabilir; her satır 'Ad: mesaj' biçiminde gelir. Cevabına yazan kişinin adıyla başla. ";
    s += u8"Türkçe yaz. Uzunluk kuralı: selam ve sıradan sohbette tek kısa cümle yaz (en çok 15 kelime); ";
    s += u8"yalnızca gerçek bir soru ya da açıklama gerekiyorsa ";
    if (compact) s += u8"2-3 cümle yaz. Hiçbir durumda " + std::to_string(max_words) + u8" kelimeyi aşma. ";
    else s += u8"konuyu tam anlat: yaklaşık " + std::to_string(max_words * 3 / 4) + u8" kelimelik, birkaç cümlelik doyurucu bir cevap yaz "
              u8"(en çok " + std::to_string(max_words) + u8" kelime); kısa kesme. ";
    s += compact ? u8"Cevap tek bir sohbet satırına sığmalı ve tam bir cümleyle bitmeli; yarım bırakma. "
                 : u8"Cevap tam bir cümleyle bitmeli; yarım bırakma. ";
    s += u8"Biçimlendirme, etiket, madde işareti veya emoji kullanma. ";
    s += u8"Oyuncular senin yapay zekâ olduğunu yeni fark edip şaşırabilir ('oha nasıl oluyor', 'bu bot mu' gibi); bunu gizleme, kısaca ve dürüstçe açıkla. ";
    s += u8"İki oyuncu birbiriyle konuşuyor ve mesaj sana yönelik değilse ya da anlamsızsa, yalnızca SKIP yaz. ";
    s += (mode >= 0 && mode < kPersonaCount) ? PersonaBody(mode) : (custom.empty() ? PersonaBody(0) : custom.c_str());
    return s;
}
// Cevabı en çok max_words kelimeye indirir ve tam cümleyle bitirir.
inline std::string LimitWords(const std::string& in, size_t max_words) {
    std::vector<std::string> words; std::string cur;
    for (char c : in) { if (IsSpace(c)) { if (!cur.empty()) words.push_back(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) words.push_back(cur);
    if (words.empty()) return {};
    auto ends = [](const std::string& w) { if (w.empty()) return false; const char c = w.back(); return c == '.' || c == '!' || c == '?'; };
    const bool cut = words.size() > max_words;
    if (cut) words.resize(max_words);
    if (cut && !ends(words.back())) { // son tam cümle sonuna geri dön (en az 3 kelime kalsın)
        for (size_t i = words.size(); i-- > 3;) if (ends(words[i - 1])) { words.resize(i); break; }
    }
    std::string out;
    for (size_t i = 0; i < words.size(); ++i) { if (i) out += ' '; out += words[i]; }
    while (!out.empty() && (out.back() == ',' || out.back() == ';' || out.back() == ':' || out.back() == '-')) out.pop_back();
    if (!out.empty() && !ends(std::string(1, out.back()))) out += '.';
    return out;
}
// ---- Mesaj süzgeçleri: gürültü, selam, başkasına yazılan satır ---------------------------------
// Küçük harfe çevirir; ç ğ ı i ö ş ü (ve büyükleri) ASCII karşılığına indirir. Diğer baytlar olduğu gibi kalır.
inline std::string Fold(const std::string& in) {
    std::string o;
    for (size_t i = 0; i < in.size(); ++i) {
        const unsigned char c = in[i];
        if (c == 0xC3 && i + 1 < in.size()) {
            const unsigned char d = in[i + 1]; char m = 0;
            switch (d) { case 0xA7: case 0x87: m = 'c'; break; case 0xB6: case 0x96: m = 'o'; break; case 0xBC: case 0x9C: m = 'u'; break; default: break; }
            if (m) { o += m; ++i; continue; }
        } else if (c == 0xC4 && i + 1 < in.size()) {
            const unsigned char d = in[i + 1]; char m = 0;
            switch (d) { case 0x9F: case 0x9E: m = 'g'; break; case 0xB1: case 0xB0: m = 'i'; break; default: break; }
            if (m) { o += m; ++i; continue; }
        } else if (c == 0xC5 && i + 1 < in.size()) {
            const unsigned char d = in[i + 1];
            if (d == 0x9F || d == 0x9E) { o += 's'; ++i; continue; }
        }
        o += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    }
    return o;
}
inline bool IsAsciiLetter(unsigned char c) { return c >= 'a' && c <= 'z'; }
// Boşluk/noktalama dışındaki "kelimeler" (folded). ASCII olmayan baytlar (emoji vb.) kelimeye katılmaz.
inline std::vector<std::string> FoldedTokens(const std::string& msg) {
    std::vector<std::string> out; std::string cur;
    for (unsigned char c : Fold(StripTags(msg))) {
        if (IsAsciiLetter(c) || (c >= '0' && c <= '9')) cur += static_cast<char>(c);
        else { if (!cur.empty()) out.push_back(cur); cur.clear(); }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
inline bool IsNoiseWord(const std::string& w) {
    for (unsigned char c : w) if (!IsAsciiLetter(c)) return false; // rakamlı kelimeler gerçek sayılır
    const size_t len = w.size(); if (len < 2) return false;
    bool seen[26] = {false}; int distinct = 0; size_t longest_run = 1, run = 1;
    bool only_ha = true, only_he = true, only_k = true, in_smash = true;
    const std::string smash = "ahsmxjkzdlfg";
    for (size_t i = 0; i < len; ++i) {
        const char c = w[i];
        if (!seen[c - 'a']) { seen[c - 'a'] = true; ++distinct; }
        if (i && w[i - 1] == c) { if (++run > longest_run) longest_run = run; } else run = 1;
        if (c != 'h' && c != 'a') only_ha = false;
        if (c != 'h' && c != 'e') only_he = false;
        if (c != 'k') only_k = false;
        if (smash.find(c) == std::string::npos) in_smash = false;
    }
    if (only_ha && len >= 3 && distinct == 2) return true;            // aha, haha, ahahah
    if (only_he && len >= 4 && distinct == 2) return true;            // hehe
    if (only_k && len >= 3) return true;                              // kkk
    if (w == "xd" || w == "xdd" || w == "xddd" || w == "lol" || w == "lmao" || w == "asd" || w == "asdf" || w == "sdf") return true;
    if (in_smash && len >= 6 && distinct <= 4) return true;           // mhshsmshsmhss, xaxhxhahhahxh
    if (longest_run >= 5) return true;                                // aaaaaa
    if (len >= 8 && static_cast<size_t>(distinct) * 3 <= len) return true;
    return false;
}
// Anlamlı hiçbir kelime yoksa (yalnız kahkaha/klavye karalaması/emoji/noktalama) true.
inline bool IsNoise(const std::string& msg) {
    const auto tokens = FoldedTokens(msg);
    for (const auto& t : tokens) if (!IsNoiseWord(t)) return false;
    return true;
}
// "sa", "s a", "SA", "sea", "selamun aleyküm" ve türevleri.
inline bool IsSalam(const std::string& msg) {
    const auto tokens = FoldedTokens(msg);
    if (tokens.empty()) return false;
    auto collapse = [](const std::string& in) { std::string o; for (char c : in) if (o.empty() || o.back() != c) o += c; return o; };
    std::string letters; for (const auto& t : tokens) letters += t;
    for (char c : letters) if (c >= '0' && c <= '9') return false;
    const std::string f = collapse(letters);
    if (f == "sa" || f == "sea") return true;
    if (tokens.size() <= 3 && (collapse(tokens[0]) == "sa" || collapse(tokens[0]) == "sea")) return true; // "sa kanka"
    for (const char* prefix : {"selamunaley", "selamnaley", "selamaley", "slmnaley", "slmunaley", "slmaley"})
        if (f.compare(0, std::string(prefix).size(), prefix) == 0) return true;
    return false;
}
inline bool NameAt(const std::string& folded, size_t at, const std::string& name) {
    if (name.empty() || folded.compare(at, name.size(), name) != 0) return false;
    const size_t end = at + name.size();
    if (end >= folded.size()) return true;
    const unsigned char n = folded[end];
    return !(IsAsciiLetter(n) || (n >= '0' && n <= '9') || n >= 0x80 || n == '_'); // ad bir kelimenin parçası olmasın
}
// Mesaj sana değil, odadaki başka bir oyuncuya mı? Başta "Ad", "Ad," veya "@Ad" varsa ya da "@Ad" geçiyorsa evet.
// Bizim adımız da geçiyorsa (bize de yazılmış olabilir) hayır.
inline bool AddressedToOther(const std::string& msg, const std::vector<std::string>& others, const std::string& self) {
    const std::string f = Trim(Fold(StripTags(msg)));
    const std::string me = Trim(Fold(StripTags(self)));
    if (!me.empty() && f.find(me) != std::string::npos) return false;
    for (const auto& o : others) {
        const std::string name = Trim(Fold(StripTags(o)));
        if (name.empty()) continue;
        size_t start = (!f.empty() && f[0] == '@') ? 1 : 0;
        if (NameAt(f, start, name)) return true;
        const std::string at = "@" + name;
        for (size_t pos = f.find(at); pos != std::string::npos; pos = f.find(at, pos + 1)) if (NameAt(f, pos + 1, name)) return true;
    }
    return false;
}
// Mesajda bizim oyun adımız geçiyor mu? Tam ad ya da adın (en az 3 harfli) bir parçası, ayrı kelime olarak.
inline bool MentionsName(const std::string& msg, const std::string& name) {
    const std::string f = Fold(StripTags(msg));
    const std::string me = Trim(Fold(StripTags(name)));
    if (me.empty() || f.empty()) return false;
    for (size_t pos = f.find(me); pos != std::string::npos; pos = f.find(me, pos + 1)) if (NameAt(f, pos, me)) return true;
    for (const auto& part : FoldedTokens(me)) {
        if (part.size() < 3) continue;
        for (size_t pos = f.find(part); pos != std::string::npos; pos = f.find(part, pos + 1)) if (NameAt(f, pos, part)) return true;
    }
    return false;
}
// Model "SKIP" yazarsa mesaj bize değildir; sohbete gönderilmez.
inline bool IsSkipReply(const std::string& reply) {
    std::string letters; for (unsigned char c : Fold(reply)) if (IsAsciiLetter(c)) letters += static_cast<char>(c);
    return letters == "skip";
}

// ---- Kalıcı ayar (API anahtarı vb.): "ad=değer" satırları; \n ve \\ kaçışlı ------------------
struct Config { std::string key, custom; int mode = 0; bool all = false; bool strict = true; int words = kMinWords; };
inline std::string EscapeLine(const std::string& v) {
    std::string o; for (char c : v) { if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n"; else if (c == '\r') {} else o += c; } return o;
}
inline std::string UnescapeLine(const std::string& v) {
    std::string o; for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == '\\' && i + 1 < v.size()) { ++i; o += v[i] == 'n' ? '\n' : v[i]; } else o += v[i];
    } return o;
}
inline std::string SerializeConfig(const Config& c) {
    return "key=" + EscapeLine(c.key) + "\nmode=" + std::to_string(c.mode) + "\nall=" + (c.all ? "1" : "0") + "\nnamefilter2=" + (c.strict ? "1" : "0") + "\nwords=" + std::to_string(c.words) + "\ncustom=" + EscapeLine(c.custom) + "\n";
}
inline Config ParseConfig(const std::string& text) {
    Config c; size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos); if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(pos, end - pos); pos = end + 1;
        const size_t eq = line.find('='); if (eq == std::string::npos) continue;
        const std::string name = line.substr(0, eq), value = UnescapeLine(line.substr(eq + 1));
        if (name == "key") c.key = Trim(value);
        else if (name == "mode") { const int m = std::atoi(value.c_str()); c.mode = (m >= 0 && m <= kPersonaCount) ? m : 0; }
        else if (name == "all") c.all = value == "1";
        else if (name == "namefilter2") c.strict = value != "0";
        else if (name == "words") c.words = ClampWords(std::atoi(value.c_str()));
        else if (name == "custom") c.custom = value;
    }
    return c;
}
}
