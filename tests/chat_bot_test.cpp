#include "chat_bot_engine.h"
#include "chat_bot_fx.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace chat_bot_core;
namespace {
int calls = 0; int fail_models = 0; int fail_code = 503; bool blocked = false;
std::string last_key, last_url, last_body;
bool FakeHttp(const std::string& url, const std::string& key, const std::string& body, int& code, std::string& response) {
    ++calls; last_key = key; last_url = url; last_body = body;
    if (calls <= fail_models) { code = fail_code; response = "{\"error\":{}}"; return fail_code != 0; }
    code = 200;
    response = blocked ? "{\"promptFeedback\":{\"blockReason\":\"SAFETY\"}}"
                       : "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Selam \\u00e7ok \\ud83d\\ude00 \\\"iyi\\\"\"}]}}]}";
    return true;
}
bool Wait(chat_bot_engine::Engine& e, chat_bot_engine::Result& r) {
    for (int i = 0; i < 400; ++i) { if (e.Poll(r)) return true; std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    return false;
}
}
int main() {
    // Etiket temizleme
    assert(StripTags("<color=#ff0000>Ali</color>: <size=20>merhaba") == "Ali: merhaba");
    assert(StripTags("a < b") == "a < b");
    // Gönderen eşleme
    assert(MatchSender("<color=#0f0>Ali</color>: selam nasılsın", "Ali").value() == "selam nasılsın");
    assert(MatchSender("[Ali]: selam", "Ali").value() == "selam");
    assert(MatchSender("Ali: selam", "ali") == std::nullopt);            // büyük/küçük harf duyarlı
    assert(MatchSender("Alice: selam", "Ali") == std::nullopt);          // ad sınırı
    assert(MatchSender("Ali selam", "Ali") == std::nullopt);             // ayırıcı yok
    assert(MatchSender("Veli: Ali: selam", "Ali") == std::nullopt);      // başka oyuncu alıntılıyor/taklit ediyor
    assert(MatchSender("Ali:", "Ali") == std::nullopt);                  // boş mesaj
    assert(MatchSender("<color=red>Şeyma</color> : merhaba", "Şeyma").value() == "merhaba"); // UTF-8 ad
    assert(MatchSender("Player123456: bana bak", "<b>Player123456</b>").value() == "bana bak");
    // Cevap temizleme: etiket enjeksiyonu, satır sonu, uzunluk
    assert(CleanReply("  <size=7000>Merhaba\n\n*dünya*  ", 180) == "size=7000Merhaba dünya");
    assert(CleanReply("a<b>c", 10) == "abc");
    assert(ScalarCount(CleanReply(std::string(500, 'a'), 180)) == 180);
    const std::string tr = "ğ"; std::string many; for (int i = 0; i < 300; ++i) many += tr;
    const std::string cut = CleanReply(many, 180);
    assert(ScalarCount(cut) == 180 && cut.size() == 360);                // çok baytlı karakter ortadan kesilmez
    assert(CleanReply("<><>", 10).empty());
    // Parçalama: cümle sonunda böl, sınırı aşma, UTF-8 bozma, en çok 4 parça
    {
        const std::string longtext = "Birinci cümle burada bitiyor. İkinci cümle biraz daha uzun ve şöyle devam ediyor, çünkü test ediyoruz! Üçüncü cümle de ğüşıöç içeriyor mu acaba? Dördüncü bitiyor.";
        const auto parts = SplitChunks(longtext, 60, 4);
        assert(parts.size() >= 3 && parts.size() <= 4);
        for (const auto& p : parts) { assert(ScalarCount(p) <= 60 && !p.empty() && p == Trim(p)); }
        assert(parts[0] == "Birinci cümle burada bitiyor.");
        assert(SplitChunks("kısa", 60, 4) == std::vector<std::string>{"kısa"});
        assert(SplitChunks(std::string(1000, 'a'), 140, 4).size() == 4);
        std::string g300; for (int i = 0; i < 300; ++i) g300 += "ğ";
        std::string joined; for (const auto& p : SplitChunks(g300, 140, 4)) { assert(ScalarCount(p) <= 140); joined += p; }
        assert(ScalarCount(joined) == 300);
    }
    // JSON
    assert(JsonEscape("a\"b\\c\nd") == "a\\\"b\\\\c\\nd");
    assert(ExtractText("{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"x\\u00e7\"}]}}]}").value() == "xç");
    assert(ExtractText("{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"\\ud83d\\ude00\"}]}}]}").value() == "\xF0\x9F\x98\x80");
    assert(!ExtractText("{\"promptFeedback\":{\"blockReason\":\"SAFETY\"}}"));
    assert(!ExtractText("{\"candidates\":[{\"text\":\"kesik"));
    // İstek: ardışık aynı rol birleşir, 2.5'te thinking kapalı
    std::vector<Turn> h = {{false, "a"}, {false, "b"}, {true, "c"}, {false, "d"}};
    const auto body = BuildRequest("sis\"tem", h, "gemini-2.5-flash-lite");
    assert(body.find("\"text\":\"a\\nb\"") != std::string::npos);
    assert(body.find("\"role\":\"model\"") != std::string::npos && body.find("sis\\\"tem") != std::string::npos);
    assert(body.find("thinkingBudget\":0") != std::string::npos);
    assert(BuildRequest("", h, "gemini-3.1-flash-lite").find("thinkingConfig") == std::string::npos);
    // Geçmiş kırpma user ile başlar
    std::vector<Turn> g = {{false, "1"}, {true, "2"}, {false, "3"}, {true, "4"}};
    TrimHistory(g, 3); assert(!g.empty() && !g.front().model && g.size() == 2 && g[0].text == "3");

    // Motor: ilk model başarılı; anahtar başlıkla gider, gövdede/URL'de değil
    chat_bot_engine::Engine e(FakeHttp);
    chat_bot_engine::Result r;
    calls = 0; fail_models = 0; blocked = false;
    assert(e.Submit("s", {{false, "selam"}}, "SECRET") && e.Busy() && !e.Submit("s", {}, "SECRET"));
    assert(Wait(e, r) && r.ok && r.text.find("çok") != std::string::npos && calls == 1 && !e.Busy());
    assert(last_key == "SECRET" && last_url.find("SECRET") == std::string::npos && last_body.find("SECRET") == std::string::npos);
    // İlk iki model 429 verir -> üçüncüye geçer (round-robin ilerler)
    e.Reset(); calls = 0; fail_models = 2; fail_code = 429;
    e.Submit("s", {{false, "x"}}, "K");
    assert(Wait(e, r) && r.ok && calls == 3 && r.model == chat_bot_engine::kModels[2] && e.Model() == chat_bot_engine::kModels[2]);
    // 503 aynı modeli bir kez tekrar dener
    e.Reset(); calls = 0; fail_models = 1; fail_code = 503;
    e.Submit("s", {{false, "x"}}, "K");
    assert(Wait(e, r) && r.ok && calls == 2 && r.model == chat_bot_engine::kModels[0]);
    // Öncelik sırası: hep en iyi model denenir; 429 alan model dinlendirilir ve sonraki istekte atlanır
    e.Reset(); calls = 0; fail_models = 0; fail_code = 429;
    e.Submit("s", {{false, "x"}}, "K"); assert(Wait(e, r) && r.ok && r.model == chat_bot_engine::kModels[0] && calls == 1);
    e.Submit("s", {{false, "x"}}, "K"); assert(Wait(e, r) && r.ok && r.model == chat_bot_engine::kModels[0]); // sticky: ilk model sağlıklıysa hep o
    e.Reset(); calls = 0; fail_models = 1; fail_code = 429;   // model0: 429 (dinlenir), model1 başarılı
    e.Submit("s", {{false, "x"}}, "K"); assert(Wait(e, r) && r.ok && r.model == chat_bot_engine::kModels[1]);
    fail_models = 0; calls = 0;
    e.Submit("s", {{false, "x"}}, "K"); assert(Wait(e, r) && r.ok && r.model == chat_bot_engine::kModels[1] && calls == 1); // model0 dinleniyor
    assert(std::string(chat_bot_engine::kModels[0]).find("3.1-flash-lite-preview") != std::string::npos);
    // Hepsi hata: all_failed, son kod
    e.Reset(); calls = 0; fail_models = 1000; fail_code = 404;
    e.Submit("s", {{false, "x"}}, "K");
    assert(Wait(e, r) && !r.ok && r.all_failed && r.code == 404 && calls == (int)chat_bot_engine::kModels.size());
    // Bağlantı yok (kod 0)
    e.Reset(); calls = 0; fail_models = 1000; fail_code = 0;
    e.Submit("s", {{false, "x"}}, "K");
    assert(Wait(e, r) && !r.ok && r.all_failed && r.code == 0);
    // Güvenlik süzgeci/boş cevap model hatası sayılmaz: diğer modellere geçilmez
    e.Reset(); calls = 0; fail_models = 0; blocked = true;
    e.Submit("s", {{false, "x"}}, "K");
    assert(Wait(e, r) && !r.ok && r.empty && !r.all_failed && calls == 1);
    // Kişilik şablonları ve 25 kelime sınırı
    {
        for (int m = 0; m < kPersonaCount; ++m) {
            const auto sys = BuildSystem(m, "");
            assert(sys.find("oyunun oda sohbetindesin") != std::string::npos && sys.find("45 kelime") != std::string::npos && sys.find("Ad: mesaj") != std::string::npos);
        }
        assert(BuildSystem(1, "").find("anı") != std::string::npos && BuildSystem(2, "").find("imam") != std::string::npos);
        assert(BuildSystem(2, "").find("soruyu gerçekten") != std::string::npos && BuildSystem(0, "").find("15 kelime") != std::string::npos);
        assert(BuildSystem(3, "Korsan gibi konuş.").find("Korsan gibi konuş.") != std::string::npos && BuildSystem(3, "").find("normal") != std::string::npos);
        assert(LimitWords("Kısa cevap burada.", 25) == "Kısa cevap burada.");
        assert(LimitWords("Merhaba nasılsın", 25) == "Merhaba nasılsın.");              // noktasız biterse eklenir
        std::string long_text; for (int i = 1; i <= 40; ++i) long_text += (i == 10 ? "kelime. " : "kelime ");
        const auto cut = LimitWords(long_text, 25);
        assert(cut == "kelime kelime kelime kelime kelime kelime kelime kelime kelime kelime." && cut.back() == '.'); // son tam cümleye döner
        std::string one; for (int i = 1; i <= 60; ++i) one += "kelime ";
        const auto hard = LimitWords(one, 25); size_t words = 1; for (char c : hard) if (c == ' ') ++words;
        assert(words == 25 && hard.back() == '.');
        assert(LimitWords("  \n ", 25).empty());
    }
    // Gürültü / selam / başkasına yazılan mesaj / SKIP
    {
        for (const char* t : {"ahahahahaah", "mhshsmshsmhss", "xaxhxhahhahxh", "hahaha", "HAHAHA", "hehe", "kkkk", "xd", "aaaaaaa", "😂😂😂", "...", "?", "ahaha xd"})
            assert(IsNoise(t));
        for (const char* t : {"merhaba", "nasılsın", "kaç yaşındasın", "tamam", "evet", "hayır", "oyun ne zaman", "sa", "kanka haha ne diyorsun", "ha", "selam", "tamamam", "ahmet nerede", "hmm", "iyi akşamlar"})
            assert(!IsNoise(t));
        for (const char* t : {"sa", "s a", "SA", "Sa", "saaa", "s.a", "S.A.", "sea", "selamun aleyküm", "Selamünaleyküm", "selamunaleykum", "selamun aleykum hocam", "SELAMUN ALEYKÜM", "sa kanka", "slm aleyküm"})
            assert(IsSalam(t));
        for (const char* t : {"selam", "merhaba", "sanane", "sa ne yapıyorsun bugün", "sana", "salak", "hasan", "", "aleyküm selam", "as"})
            assert(!IsSalam(t));
        const std::vector<std::string> others = {"Ali", "Şeyma", "Veli"};
        assert(AddressedToOther("Ali nasılsın", others, "Ben"));
        assert(AddressedToOther("ali, bak şuna", others, "Ben"));
        assert(AddressedToOther("@Veli gel", others, "Ben"));
        assert(AddressedToOther("şeyma naber", others, "Ben"));                 // Ş/ş katlanır
        assert(AddressedToOther("tamam @Ali bir bak", others, "Ben"));
        assert(!AddressedToOther("Alice nasılsın", others, "Ben"));             // ad kelimenin parçası
        assert(!AddressedToOther("nasılsın bakalım", others, "Ben"));
        assert(!AddressedToOther("Ali ve Ben nasılsınız", others, "Ben"));      // bize de yazılmış
        assert(!AddressedToOther("ali", {}, "Ben"));
        assert(IsSkipReply("SKIP") && IsSkipReply("skip.") && IsSkipReply(" Skip! ") && !IsSkipReply("Skipper geldi"));
        assert(BuildSystem(0, "").find("SKIP") != std::string::npos);
        assert(Fold("ÇAĞRI İŞÖÜ") == "cagri isou");
        assert(MentionsName("Ahmet nasılsın", "Ahmet_TR") && MentionsName("ahmet bak", "Ahmet_TR") && MentionsName("hey Ahmet_TR", "Ahmet_TR"));
        assert(!MentionsName("ahmetler geldi", "Ahmet_TR") && !MentionsName("nasılsın", "Ahmet_TR") && !MentionsName("merhaba", ""));
        assert(MentionsName("Şeyma kız", "Şeyma") && MentionsName("<b>Player123456</b> bak", "Player123456"));
        assert(!MentionsName("ab nasıl", "Ab_Cd"));                                  // 3 harften kısa parça sayılmaz
        assert(ParseConfig("").strict && !ParseConfig("namefilter2=0\n").strict && ParseConfig("namefilter=0\n").strict && ParseConfig(SerializeConfig([]{Config c;c.strict=false;return c;}())).strict == false);
        assert(ParseConfig("").words == 45 && ParseConfig("words=100\n").words == 100 && ParseConfig("words=5\n").words == 45 && ParseConfig("words=999\n").words == 150);
        assert(BuildSystem(0, "", 120).find("en çok 120 kelime") != std::string::npos && BuildSystem(0, "", 120).find("yaklaşık 90 kelime") != std::string::npos && BuildSystem(2, "", 120).find("kısa kesme") != std::string::npos);
        assert(BuildSystem(2, "").find("bilen birine sor") != std::string::npos && BuildSystem(2, "").find("son çare") != std::string::npos && BuildSystem(2, "").find("bilen bir hocaya") == std::string::npos);
    }
    for(const char* message:{"zeka bir sorum var", "yapay bunu açıkla", "yapayzeka neden böyle", "yapay zeka merhaba", "ZEKA 2+2 kaç", "<b>YapayZeka</b>, nasılsın?"})
        assert(MentionsBot(message,"Bot"));
    for(const char* message:{"zekalı insan", "yapaylık var", "yapayzekanın cevabı", "altzeka", "sıradan soru"})
        assert(!MentionsBot(message,"Bot"));
    assert(MentionsBot("zeka bir soru","")&&MentionsBot("Bot bir soru","Bot"));
    assert(SmallTalkReply("yapay zeka merhaba","Ali","Bot"));
    assert(SmallTalkReply("merhaba yapay zeka","Ali","Bot"));
    assert(DirectedProfanity("yapayzeka seni sikeyim","Bot",{},false));
    // Control output stays internal, including name-prefixed and Turkish forms.
    for(const char* skip:{"SKIP","skıp",u8"SKİP","**SKIP**","[SKIP]","Ali: SKIP","Ali, skip.","Ali SKIP","<b>SKIP</b>"}){
        assert(IsSkipReply(skip,"Ali"));assert(PrepareReply(skip,45,"Ali").empty());
    }
    assert(PrepareReply("asdfasdfasdf",45,"Ali").empty());
    assert(!PrepareReply("Ali, Skipper geldi.",45,"Ali").empty());
    assert(!PrepareReply("Ali, skip tuşu sonraki bölüme geçer.",45,"Ali").empty());
    for(const char* greeting:{"merhaba","MERHABAAA!!!","slm","selam","sa","S.A.","nasılsınız","nasilsin","selam nasılsınız arkadaşlar", "Bot nasılsın?", "@Bot slm", "merhaba Bot", "selamun aleyküm", "slm aleyküm", "iyi akşamlar", "günaydın"}){
        const auto local=SmallTalkReply(greeting,"Ali","Bot");assert(local&&!local->empty());
    }
    assert(SmallTalkReply("sa","Ali","Bot",2)->find("rahmetullahi")!=std::string::npos);
    for(const char* question:{"merhaba, oyun nasıl oynanır?", "sa 2+2 kaç", "nasılsın ve dünya neden yuvarlak", "Bot selam, güneş kaç derece", "merhaba hava bugün nasıl", "selamun aleykum bu oyun nasıl oynanır", "nasilsiniz bu iş nasıl yapılır"})
        assert(!SmallTalkReply(question,"Ali","Bot"));
    for(const char* insult:{"Bot seni sikeyim","Bot siktir git","Bot orospu çocuğu","Bot sen piçsin","Bot amına koyayım"})
        assert(DirectedProfanity(insult,"Bot",{"Ali"},false));
    assert(DirectedProfanity("seni sikeyim","Bot",{"Ali"},true));
    for(const char* slang:{"Bot lan nasılsın","Bot amk oyun açılmıyor","Bot aq","Bot salak mısın","Bot mal","Bot sikayetim var", "Bot sıkıldım"})
        assert(!DirectedProfanity(slang,"Bot",{"Ali"},true));
    assert(!DirectedProfanity("Ali seni sikeyim","Bot",{"Ali"},true));
    assert(!DirectedProfanity("Bot siktir kelimesi ne demek","Bot",{},true));
    assert(!DirectedProfanity("Bot bana orospu dedi","Bot",{},true));
    assert(!DirectedProfanity("seni sikeyim","Bot",{},false));
    assert(ProfanityWarnings("Ali").size()==2);
    assert(!NameReminderText("<color=red>Bot</color>").empty());
    assert(NameReminderText("").empty());
    NameReminder clock;
    clock.Update(100,true,true,1,"Bot",true);assert(!clock.Due(114.99)&&clock.Due(115));
    clock.Queued(115);assert(!clock.Due(234.99)&&clock.Due(235));
    clock.Update(236,true,false,1,"Bot",true);assert(!clock.Due(10000));
    clock.Update(240,true,true,1,"Bot",true);assert(!clock.Due(254)&&clock.Due(255));
    clock.Update(255,true,true,2,"Bot",true);assert(!clock.Due(269)&&clock.Due(270));
    clock.Update(270,true,true,2,"YeniBot",true);assert(!clock.Due(284)&&clock.Due(285));
    clock.Update(285,true,true,2,"YeniBot",false);assert(!clock.Due(10000));
    clock.Update(300,false,true,2,"YeniBot",true);assert(!clock.Due(10000));
    clock.Update(301,true,true,0,"YeniBot",true);assert(!clock.Due(10000));
    clock.Update(302,true,true,2,"",true);assert(!clock.Due(10000));
    for(auto event:{chat_bot_fx::Event::Start,chat_bot_fx::Event::Stop,chat_bot_fx::Event::Profile,chat_bot_fx::Event::Model}){
        const auto lines=chat_bot_fx::Lines(event,"test-model",true);
        assert(lines.size()>=18&&lines.size()<=24&&lines.front()==std::string("<#888F>")+chat_shortcut::kPayload);
        assert((lines.size()-1)*chat_bot_fx::kInterval<chat_bot_fx::kTimeout);
        for(const auto& line:lines)assert(ScalarCount(line)<=200);
    }
    for(auto event:{chat_bot_fx::Event::Start,chat_bot_fx::Event::Stop,chat_bot_fx::Event::Model}){
        const auto lines=chat_bot_fx::Lines(event,"SECRET_PROVIDER_ID",true);
        for(const auto& line:lines){
            assert(line.find("SECRET_PROVIDER_ID")==std::string::npos);
            assert(line.find("IL2CPP")==std::string::npos&&line.find("ChatManager")==std::string::npos);
        }
    }
    assert(chat_bot_fx::Lines(chat_bot_fx::Event::Profile,u8"İmam",true).back().find(u8"MODEL: İMAM")!=std::string::npos);
    // Kalıcı ayar
    {
        Config c; c.key = "AIza\\x"; c.mode = 2; c.all = true; c.custom = "satır1\nsatır2";
        const auto back = ParseConfig(SerializeConfig(c));
        assert(back.key == c.key && back.mode == 2 && back.all && back.custom == c.custom);
        assert(ParseConfig("mode=9\n").mode == 0 && ParseConfig("saçma\n").key.empty() && !ParseConfig("").all);
        assert(SerializeConfig(c).find('\r') == std::string::npos);
    }
    std::cout << "Chat bot: tag stripping, sender matching, reply sanitising, Gemini JSON, model failover (key only in header): PASS\n";
}
