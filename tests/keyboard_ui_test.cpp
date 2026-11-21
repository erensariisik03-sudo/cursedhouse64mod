#include "keyboard_ui.h"
#include "keyboard_catalog.h"
#include "chat_shortcut.h"
#include "lobby_emote_bridge.h"
#include "auto_join_bridge.h"
#include "gameplay_bridge.h"
#include "text_buffer.h"
#include "native_shortcuts.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
static std::map<ImGuiID,ImRect> rects;
static std::map<std::string,ImRect> items;
void ImGuiTestEngineHook_ItemAdd(ImGuiContext*,ImGuiID id,const ImRect& bb,const ImGuiLastItemData*) { rects[id]=bb; }
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID id,const char* label,ImGuiItemStatusFlags) {
 auto it=rects.find(id);if(it!=rects.end()) items[label]=it->second;
}
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID) { return ""; }
void Frame() { rects.clear();items.clear();ImGui::NewFrame();keyboard_ui::Draw();ImGui::Render(); }
void Set(const char* text) { keyboard_ui::SetText(text);Frame();Frame();Frame(); }
void Click(const char* label) {
 if(!items.count(label)) { std::cerr<<"Missing key "<<label<<"\n"; std::abort(); }
 auto pos=items.at(label).GetCenter();auto& io=ImGui::GetIO();
 io.AddMousePosEvent(pos.x,pos.y);Frame();io.AddMouseButtonEvent(0,true);Frame();
 io.AddMouseButtonEvent(0,false);Frame();Frame();Frame();Frame();
}
void Direct(const char* label){
 assert(items.count(label));const auto p=items.at(label).GetCenter();
 assert(keyboard_ui::TouchDown(p.x,p.y));keyboard_ui::TouchUp();
}
void Key(ImGuiKey key) { auto& io=ImGui::GetIO();io.AddKeyEvent(key,true);Frame();io.AddKeyEvent(key,false);Frame(); }
void Expect(const std::string& text) {
 if(text!=keyboard_ui::GetText()) { std::cerr<<"Expected ["<<text<<"] got ["<<keyboard_ui::GetText()<<"]\n";std::abort(); }
}
void LayoutCheck(ImVec2 size) {
 auto* window=ImGui::FindWindowByName("##keyboard");
 assert(window->Pos.y>=0 && window->Size.y<=size.y*(size.y<500?0.63f:(size.y>size.x?0.44f:0.53f)));
 for(const auto& item:items) {
  if(item.first.find("##")!=std::string::npos) continue;
  const auto bb=item.second;
  if(bb.Min.x<0||bb.Min.y<0||bb.Max.x>size.x+1||bb.Max.y>size.y+1) { std::cerr<<"Offscreen "<<item.first<<"\n";std::abort(); }
 }
 for(auto label:{"Boyut","Renk","Emoji","Delete","Clear","Enter","Gizle"}) if(items.count(label)) {
  const auto bb=items.at(label);
  if(ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().FramePadding.x>bb.GetWidth()+1) {
   std::cerr<<"Clipped label "<<label<<" width="<<bb.GetWidth()<<" text="<<ImGui::CalcTextSize(label).x<<" on "<<size.x<<"x"<<size.y<<"\n";std::abort();
  }
 }
}
int main() {
 assert(lobby_emote_bridge::Read().visible&&!lobby_emote_bridge::Read().ready);
 for(auto size:{ImVec2(1280,800),ImVec2(1920,1080),ImVec2(800,360),ImVec2(360,800),ImVec2(640,360)}) {
  ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=size;io.DeltaTime=1.f/60;
  ImGui::GetCurrentContext()->TestEngineHookItems=true;
  float scale=std::clamp(std::min(size.x,size.y)/500.f,0.7f,1.6f);
  auto& style=ImGui::GetStyle();style.ItemSpacing=ImVec2(3,3);style.WindowPadding=ImVec2(6,6);style.ScaleAllSizes(scale);
  ImFontConfig config;config.SizePixels=std::round(15*scale);io.Fonts->AddFontDefault(&config);
  unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
  assert(pixels&&w>0&&h>0);
  io.Fonts->ClearTexData();assert(!io.Fonts->TexPixelsAlpha8&&!io.Fonts->TexPixelsRGBA32&&io.Fonts->IsBuilt());
  keyboard_ui::ResetTransientState();keyboard_ui::SetIntegrated(true);keyboard_ui::SetPassword(false);
  keyboard_ui::SetVisible(true);Set("");LayoutCheck(size);
  assert(!items.count("</size>") && !items.count("</color>"));
  assert(ImGui::FindWindowByName("##keyboard")->Size.x >= size.x*0.975f);
  assert(ImGui::FindWindowByName("##keyboard")->Size.y >= size.y*(size.y<500?0.60f:(size.y>size.x?0.41f:0.50f)));
  assert(items.at("Emoji").Min.y < items.at("##keyboard_text").Min.y);
  // Reproduce the old TouchScreen queue backlog for the same physical key sequence.
  Set("");io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  for(auto label:{"q","w","e","r","t","y"}){
   const auto p=items.at(label).GetCenter();io.AddMousePosEvent(p.x,p.y);
   io.AddMouseButtonEvent(0,true);io.AddMouseButtonEvent(0,false);
  }
  int legacy_frames=0;while(!ImGui::GetCurrentContext()->InputEventsQueue.empty()&&legacy_frames<80){Frame();++legacy_frames;}
  Frame();Expect("qwerty");assert(legacy_frames>=12);
  std::cout<<"Touch queue benchmark "<<size.x<<"x"<<size.y<<": 6 taps legacy="<<legacy_frames<<" frames, direct=0 extra render frames\n";
  // A whole burst must commit without any additional ImGui/render frame.
  // Old mouse-source trickling needed separate hover/down/up frames per touch.
  Set("");const int frame=ImGui::GetFrameCount();
  std::string burst;
  for(int n=0;n<24;++n)for(auto label:{"q","w","e","r","t","y"}){Direct(label);burst+=label;Expect(burst);}
  assert(ImGui::GetFrameCount()==frame&&ImGui::GetCurrentContext()->InputEventsQueue.empty());
  Frame();Expect(burst);
  Set("");Direct("Shift");Direct("a");Direct("b");Expect("Ab");Frame();
  // One held key must not repeat when sliding across other character keys.
  Set("");auto fast=items.at("q").GetCenter();assert(keyboard_ui::TouchDown(fast.x,fast.y));
  const auto other=items.at("w").GetCenter();
  for(int held=0;held<20;++held){keyboard_ui::TouchHeld(other.x,other.y);Frame();}
  Expect("q");keyboard_ui::TouchUp();
  Set("abcdefghijk");fast=items.at("Delete").GetCenter();assert(keyboard_ui::TouchDown(fast.x,fast.y));Expect("abcdefghij");
  for(int held=0;held<30;++held){Frame();keyboard_ui::TouchHeld(fast.x,fast.y);}
  assert(text_buffer::Count(keyboard_ui::GetText())<10);keyboard_ui::TouchUp();
  Set("");Direct(u8"█");Expect(chat_shortcut::kPayload);assert(keyboard_ui::ConsumeQuickSend()&&!keyboard_ui::ConsumeQuickSend());
  Set("");Direct("Boyut");Frame();fast=items.at("Tamam").GetCenter();
  assert(!keyboard_ui::TouchDown(fast.x,fast.y));keyboard_ui::TouchUp();
  Direct("1");Direct("2");Frame();Direct("Tamam");Expect("<size=12>");Frame();
  keyboard_ui::ResetTransientState();Set("");
  // The character must appear on DOWN, before any release event.
  Set("");auto press=items.at("a").GetCenter();
  io.AddMousePosEvent(press.x,press.y);Frame();io.AddMouseButtonEvent(0,true);Frame();Expect("a");
  for(int held=0;held<12;++held)Frame();Expect("a");
  auto over=items.at("b").GetCenter();io.AddMousePosEvent(over.x,over.y);Frame();Expect("a");
  io.AddMouseButtonEvent(0,false);Frame();Frame();Frame();Expect("a");
  // Repeated taps have no release-triggered duplicates, and retain order.
  Set("");for(auto label:{"q","w","e","r","t","y"}) {
   const auto center=items.at(label).GetCenter();io.AddMousePosEvent(center.x,center.y);Frame();
   io.AddMouseButtonEvent(0,true);Frame();io.AddMouseButtonEvent(0,false);Frame();
  }
  Frame();Frame();Expect("qwerty");
  Set("abcdefghijklmno");press=items.at("Delete").GetCenter();
  io.AddMousePosEvent(press.x,press.y);Frame();io.AddMouseButtonEvent(0,true);Frame();
  Expect("abcdefghijklmn");
  for(int held=0;held<45;++held)Frame();
  assert(text_buffer::Count(keyboard_ui::GetText())<10);
  io.AddMouseButtonEvent(0,false);Frame();Frame();Frame();
  Set("draft");assert(!keyboard_ui::ReconcileText("older","cleaned"));Expect("draft");
  assert(keyboard_ui::ReconcileText("draft","accepted"));Frame();Expect("accepted");
  Click("Boyut");assert(!keyboard_ui::ReconcileText(keyboard_ui::GetText(),"cleaned"));Click(u8"Vazgeç");Expect("accepted");
  Set("");Click("a");Click("Shift");Click("A");Expect("aA");
  Key(ImGuiKey_LeftArrow);Click("b");Expect("abA");
  io.AddKeyEvent(ImGuiMod_Ctrl,true);Key(ImGuiKey_A);io.AddKeyEvent(ImGuiMod_Ctrl,false);Frame();
  Click("c");Expect("c");Click("Clear");Expect("");
  Click("<");Click(">");Expect("<>");Click(u8"█");Expect(chat_shortcut::kPayload);
  assert(keyboard_ui::ConsumeQuickSend()&&!keyboard_ui::ConsumeQuickSend()&&!keyboard_ui::ConsumeSubmit()&&keyboard_ui::IsVisible());
  Click("Clear");Click("#+=");LayoutCheck(size);
  const char* symbols[]={"|","\\","}","]","[","{",u8"½","$","#",u8"£","_","?","=",")","(","/","&","%","+","^","'","<",">"};
  std::string expected;
  for(auto key:symbols) { Click(key);expected+=key;Expect(expected); }
  Click(u8"█");Expect(chat_shortcut::kPayload);assert(keyboard_ui::ConsumeQuickSend()&&!keyboard_ui::ConsumeQuickSend()&&!keyboard_ui::ConsumeSubmit());
  Click("ABC123");assert(items.count("q"));
  Set("");Click("Boyut");Expect("<size=xxx>");assert(std::string(keyboard_ui::GetCommittedText()).empty());LayoutCheck(size);
  Click("1");Click("1");Expect("<size=11>");assert(std::string(keyboard_ui::GetCommittedText()).empty());Click("Tamam");assert(std::string(keyboard_ui::GetCommittedText())=="<size=11>");Click("x");Expect("<size=11>x");
  Set("abc");Key(ImGuiKey_LeftArrow);Click("Boyut");Expect("ab<size=xxx>c");
  for(int i=0;i<4;++i) Click("9");Expect("ab<size=9999>c");Click("Tamam");Click("z");Expect("ab<size=9999>zc");
  Click("Boyut");Click("2");Click(u8"Vazgeç");Expect("ab<size=9999>zc");
  Set("");Click("Boyut");io.AddInputCharactersUTF8("12a34");Frame();Frame();Expect("<size=1234>");
  Key(ImGuiKey_Enter);Expect("<size=1234>");assert(keyboard_ui::ConsumeSubmit()==0);assert(items.count("q"));
  for(const auto& color:keyboard_catalog::kColors) {
   Set("");Click("Renk");Expect("<xxxx>");assert(std::string(keyboard_ui::GetCommittedText()).empty());LayoutCheck(size);
   Click(color.name);Expect(std::string("<#")+color.rgba+">");assert(items.count("q"));
  }
  Set("keep");Click("Renk");Click(u8"Vazgeç");Expect("keep");
  Set("");Click("Emoji");assert(keyboard_ui::ConsumeEmojiRequest());assert(!keyboard_ui::ConsumeEmojiRequest());
  assert(!items.count(u8"Gülümse"));
  keyboard_ui::SetEmojiSupport((uint64_t{1}<<0)|(uint64_t{1}<<26)|(uint64_t{1}<<44),true);Frame();Frame();LayoutCheck(size);
  Click(u8"Gülümse");Click("Mutlu");Click("Hayalet");Expect(u8"☺😀👻");assert(!items.count(u8"Beğen"));
  Click("ABC123");assert(items.count("q"));
  Set(u8"a😀ş");Click("Delete");Expect(u8"a😀");Click("Delete");Expect("a");
  std::string full(1024,'x');Set(full.c_str());Click("a");Expect(full);
  Click("Boyut");Expect(full);assert(items.count("q"));Click("Renk");Expect(full);assert(items.count("q"));
  Click("Delete");Click("a");assert(text_buffer::Count(keyboard_ui::GetText())==1024);
  std::string almost(1023,'x');Set(almost.c_str());io.AddInputCharactersUTF8(u8"şö");Frame();Frame();Expect(almost+u8"ş");
  keyboard_ui::SetPassword(true);Set("secret");
  auto* state=ImGui::GetInputTextState(ImGui::FindWindowByName("##keyboard")->GetID("##keyboard_text"));
  assert(state && (state->Flags&ImGuiInputTextFlags_Password));
  keyboard_ui::ResetTransientState();keyboard_ui::SetPassword(false);Set("safe");
  io.AddKeyEvent(ImGuiMod_Ctrl,true);Key(ImGuiKey_Z);io.AddKeyEvent(ImGuiMod_Ctrl,false);Frame();Expect("safe");
  Click("Enter");assert(keyboard_ui::ConsumeSubmit()==1);assert(keyboard_ui::ConsumeSubmit()==0);
  Click("Renk");Click("Gizle");Expect("safe");assert(!keyboard_ui::IsVisible());Frame();assert(!items.count("Keyboard"));
  // Yuvarlak sürüklenebilir menü düğmesi: kapalıyken yalnız düğme; dokununca AI/Kıyafet sekmeleri; sürükleme açıp kapatmaz.
  Frame();Frame();if(items.count("AI")){Click("##hub_btn");}
  assert(items.count("##hub_btn")&&!items.count("AI")&&!items.count("q")&&!items.count("Dans >")&&!items.count("Reklam"));
  auto hit=items.at("##hub_btn").GetCenter();assert(keyboard_ui::HitTest(hit.x,hit.y));assert(!keyboard_ui::HitTest(1,1));
  {   // sürükle: konum değişir, panel açılmaz
    auto& mio=ImGui::GetIO();const auto start=items.at("##hub_btn").GetCenter();
    mio.AddMousePosEvent(start.x,start.y);Frame();mio.AddMouseButtonEvent(0,true);Frame();
    for(int i=1;i<=6;++i){mio.AddMousePosEvent(start.x+i*10,start.y+i*7);Frame();}
    mio.AddMouseButtonEvent(0,false);Frame();Frame();Frame();
    const auto moved=items.at("##hub_btn").GetCenter();
    assert(std::fabs(moved.x-start.x)>40&&std::fabs(moved.y-start.y)>25&&!items.count("AI"));
  }
  Click("##hub_btn");Frame();Frame();
  assert(items.count("AI")&&items.count("Kıyafet")&&!items.count("Reklam"));
  Direct("Kıyafet");Frame();Frame();assert(items.count("Çık-gir: herkese uygula"));
  Direct("AI");Frame();Frame();assert(items.count("API anahtarı gir")&&!items.count("Çık-gir: herkese uygula"));
  // Oyun paneli: bölüm listesi -> tek bölüm -> geriyle tam liste.
  Direct("Oyun");Frame();Frame();
  for(auto label:{"Eşya spawn","Düşman durdur","Düşman öldür","God Mode"})assert(items.count(label));
  Direct("Düşman durdur");Frame();Frame();
  assert(items.count(u8"← Bölümlere dön")&&items.count("Granny"));
  assert(!items.count("Eşya spawn")&&!items.count("Düşman öldür")&&!items.count("God Mode"));
  Direct(u8"← Bölümlere dön");Frame();Frame();
  for(auto label:{"Eşya spawn","Düşman durdur","Düşman öldür","God Mode"})assert(items.count(label));
  Direct("Eşya spawn");Frame();Frame();
  assert(items.count("Seçili eşyayı önüme oluştur")&&!items.count("Düşman durdur"));
  { // The action stays visible above an overflowing catalogue on every tested screen.
    gameplay_bridge::Snapshot ready;ready.context=42;ready.ready=ready.spawn_ready=true;ready.status="Hazır.";
    for(int i=1;i<=20;++i)ready.items.push_back({i,"Prefab "+std::to_string(i)});
    gameplay_bridge::Publish(ready);Frame();Frame();Frame();
    assert(items.count("Seçili eşyayı önüme oluştur"));
    const auto action=items.at("Seçili eşyayı önüme oluştur");
    const auto* hub=ImGui::FindWindowByName("##hub");
    assert(action.Min.y>=hub->ClipRect.Min.y&&action.Max.y<=hub->ClipRect.Max.y);
    Direct("Seçili eşyayı önüme oluştur");
    const auto pending=gameplay_bridge::Take();
    assert(pending.size()==1&&pending.front().kind==gameplay_bridge::Kind::Spawn&&pending.front().context==42&&pending.front().item==1);
    gameplay_bridge::Publish({});Frame();Frame();
  }
  Direct(u8"← Bölümlere dön");Frame();Frame();
  Direct("AI");Frame();Frame();
  keyboard_ui::SetVisible(true);Frame();assert(!items.count("##hub_btn")&&!items.count("AI")&&items.count("q"));
  keyboard_ui::SetVisible(false);Frame();Frame();assert(items.count("##hub_btn")&&!items.count("Dans >"));
  if(items.count("AI")){Click("##hub_btn");Frame();}
  {   // "Game full" sonrası yalnız o durumda görünen otomatik bağlan düğmesi
    Frame();assert(!items.count("Otomatik bağlan (oda boşalınca)")&&!items.count("Vazgeç"));
    auto_join_bridge::Snapshot offer;offer.offer=true;offer.room="Oda1";auto_join_bridge::Publish(offer);Frame();Frame();
    assert(items.count("Otomatik bağlan (oda boşalınca)"));
    Direct("Otomatik bağlan (oda boşalınca)");assert(auto_join_bridge::Take().start);
    offer.offer=false;offer.active=true;offer.attempts=3;auto_join_bridge::Publish(offer);Frame();Frame();
    assert(items.count("Vazgeç")&&!items.count("Otomatik bağlan (oda boşalınca)"));
    Direct("Vazgeç");assert(auto_join_bridge::Take().cancel);
    auto_join_bridge::Publish({});Frame();Frame();assert(!items.count("Vazgeç"));
  }
  // Hybrid mode renders only shortcuts; device text stays authoritative and
  // buttons queue operations, never the stale copy displayed by a previous frame.
  const auto session=native_shortcuts::Begin();
  native_shortcuts::Publish(session,"Android draft",13,13,.625f);
  keyboard_ui::ResetTransientState();keyboard_ui::SetShortcutsOnly(true);
  keyboard_ui::SetVisible(true);Frame();Frame();Frame();
  assert(items.count("Boyut")&&items.count("Renk")&&!items.count("Emoji")&&!items.count("q")&&!items.count("##keyboard_text"));
  auto* strip=ImGui::FindWindowByName("##native_shortcuts");
  assert(strip&&strip->Pos.y>=0&&strip->Pos.y+strip->Size.y<=size.y*.625f);
  Direct("<");Direct(">");
  auto commands=native_shortcuts::Take(session);
  assert(commands.size()==2&&commands[0].text=="<"&&commands[1].text==">");
  assert(native_shortcuts::Read().text=="Android draft");
  Direct("Boyut");Frame();Direct("1");Direct("2");Frame();
  assert(native_shortcuts::Take(session).empty()); // unfinished tags never leak
  native_shortcuts::Publish(session,"newer Android word",18,18,.625f);
  Frame();Direct("Tamam");Frame();
  commands=native_shortcuts::Take(session);
  assert(commands.size()==1&&commands[0].text=="<size=12>"&&commands[0].kind==native_shortcuts::Kind::Insert);
  assert(native_shortcuts::Read().text=="newer Android word");
  Direct("Renk");Frame();Direct(u8"Kırmızı");Frame();
  commands=native_shortcuts::Take(session);assert(commands.size()==1&&commands[0].text=="<#F00F>");
  Direct("Renk");Frame();Direct(u8"Vazgeç");Frame();assert(native_shortcuts::Take(session).empty());
  Direct(u8"█");
  commands=native_shortcuts::Take(session);
  assert(commands.size()==1&&commands[0].kind==native_shortcuts::Kind::ReplaceAll&&commands[0].quick_send&&commands[0].text==chat_shortcut::kPayload);
  Frame();Direct(u8"Küçült");Frame();
  assert(keyboard_ui::IsVisible()&&items.count(u8"Kısayollar")&&!items.count("Boyut"));
  Direct(u8"Kısayollar");Frame();assert(items.count("Boyut"));
  keyboard_ui::SetPassword(true);Frame();
  const auto disabled=items.at("<").GetCenter();assert(!keyboard_ui::TouchDown(disabled.x,disabled.y));
  assert(native_shortcuts::Take(session).empty());keyboard_ui::SetPassword(false);
  native_shortcuts::Publish(session,"draft",5,5,0);Frame();
  assert(ImGui::FindWindowByName("##native_shortcuts")->Pos.y==4.f);
  native_shortcuts::End(session);keyboard_ui::SetShortcutsOnly(false);keyboard_ui::SetVisible(false);
  // Context/device recreation can request pixels again after the CPU copy was freed.
  io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);assert(pixels&&w>0&&h>0);Frame();
  ImGui::DestroyContext();
 }
 std::cout<<"Press-on-down typing, held Delete and taller ImGui layouts, all symbols/block, size/color shortcuts, filtered emoji, cursor, 1024 limit and submit: PASS\n";
}
