#include "keyboard_ui.h"
#include "text_buffer.h"
#include "shortcut_editor.h"
#include "native_shortcuts.h"
#include "keyboard_catalog.h"
#include "chat_shortcut.h"
#include "lobby_emote_bridge.h"
#include "chat_bot_bridge.h"
#include "chat_bot_core.h"
#include "auto_join_bridge.h"
#include "wardrobe_bridge.h"
#include "gameplay_bridge.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

namespace {
char g_text[text_buffer::kCapacity] = "";
bool g_visible = false, g_integrated = false, g_password = false;
bool g_shift = false, g_caps = false, g_submit = false;
bool g_quick_send=false;
bool g_shortcuts_only=false,g_strip_collapsed=false;
uint64_t g_native_session=0;
float g_native_bottom=0;
bool g_text_dirty = false, g_restore_cursor = false, g_reset_history = true, g_refocus = false;
int g_cursor = 0, g_anchor = 0;
int g_rect_count = 0;
ImVec2 g_rect_min[3], g_rect_max[3];
enum class Page { Letters, Symbols, Emoji };
Page g_page = Page::Letters;
shortcut_editor::Editor g_shortcut;
uint64_t g_emoji_mask = 0;
bool g_emoji_ready = false, g_emoji_query_ok = false, g_emoji_request = false;
int g_emoji_page = 0;
double g_last_shift = -1.0, g_limit_until = -1.0;
struct TouchKey { ImVec2 min,max;std::function<void()> action;bool repeat; };
std::vector<TouchKey> g_touch_keys;
std::function<void()> g_touch_repeat;
ImVec2 g_repeat_min{},g_repeat_max{};
double g_repeat_at=0;
size_t g_wardrobe_tab=0;
keyboard_ui::PasteProvider g_paste=nullptr;
struct Layout { float unit, gap, height; };
void InvalidateKeys(){g_touch_keys.clear();g_touch_repeat={};}

float Width(const Layout& l, float units) { return units * l.unit + (units - 1.0f) * l.gap; }
void Dirty() { g_text_dirty = g_restore_cursor = true; }
void Limit() { g_limit_until = ImGui::GetTime() + 1.0; }
bool Upper() { return g_caps != g_shift; }
void CancelShortcut() { InvalidateKeys(); g_shortcut.Cancel(g_text, g_cursor, g_anchor); Dirty(); }
bool QueueNative(const char* text,bool replace=false,bool quick=false) {
    return native_shortcuts::Queue(g_native_session,{replace?native_shortcuts::Kind::ReplaceAll:native_shortcuts::Kind::Insert,text,quick});
}
bool CommitSize() {
    const std::string tag=g_shortcut.Tag();
    if(!g_shortcut.CommitSize(g_cursor,g_anchor))return false;
    if(g_shortcuts_only&&!QueueNative(tag.c_str()))Limit();
    Dirty();return true;
}
bool PickColor(const char* rgba) {
    if(!g_shortcut.PickColor(rgba,g_text,g_cursor,g_anchor))return false;
    if(g_shortcuts_only&&!QueueNative((std::string("<#")+rgba+">").c_str()))Limit();
    return true;
}
void FinishShortcut() {
    if(g_shortcut.HasSize())CommitSize();
    else if(g_shortcut.Active())g_shortcut.Cancel(g_text,g_cursor,g_anchor);
    Dirty();
}
void HideEditor() {
    InvalidateKeys();
    if(g_shortcuts_only){g_strip_collapsed=true;g_page=Page::Letters;}
    else g_visible=false;
}
void Insert(const char* value) {
    if(g_shortcut.Active())FinishShortcut();
    if(g_shortcuts_only){if(!QueueNative(value))Limit();return;}
    if(text_buffer::Replace(g_text,g_cursor,g_anchor,value))Dirty();
    else if(text_buffer::Count(g_text)>=text_buffer::kLimit)Limit();
}
// Pano metni: satır sonları/sekme boşluğa çevrilir, baştaki/sondaki boşluk atılır,
// kontrol karakterleri düşer. Sınır (1024) Insert/text_buffer tarafından uygulanır.
std::string CleanPaste(const std::string& in) {
    std::string out;
    for (unsigned char c : in) {
        if (c == '\r' || c == '\n' || c == '\t') c = ' ';
        if (c < 0x20 || c == 0x7F) continue;
        out += static_cast<char>(c);
    }
    size_t a = 0, b = out.size();
    while (a < b && out[a] == ' ') ++a;
    while (b > a && out[b - 1] == ' ') --b;
    return out.substr(a, b - a);
}
void Paste() {
    std::string text;
    if (!g_paste || !g_paste(text)) { Limit(); return; }
    const std::string clean = CleanPaste(text);
    if (clean.empty()) { Limit(); return; }
    Insert(clean.c_str());
}
void Backspace() {
    if (text_buffer::Replace(g_text, g_cursor, g_anchor, "", true)) Dirty();
}
bool Key(const char* label, float width, float height, bool active = false, bool repeat = false) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f,0.35f,0.39f,.84f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f,0.44f,0.48f,.91f));
    }
    if (repeat) ImGui::PushButtonRepeat(true);
    // Commit on DOWN: ordinary typing no longer waits for a finger release.
    // Keep ActiveId ownership while held so moving across keys does not type twice.
    const bool pressed = ImGui::ButtonEx(label, ImVec2(width, height), ImGuiButtonFlags_PressedOnClick);
    if (pressed) g_refocus = true;
    if (repeat) ImGui::PopButtonRepeat();
    if (active) ImGui::PopStyleColor(2);
    return pressed;
}
void ActionKey(const char* label,float width,float height,std::function<void()> action,bool active=false,bool repeat=false){
    const bool pressed=Key(label,width,height,active,repeat);
    if(!(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)){
        ImRect hit(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());hit.ClipWith(ImGui::GetCurrentWindow()->ClipRect);
        if(hit.Min.x<hit.Max.x&&hit.Min.y<hit.Max.y)g_touch_keys.push_back({hit.Min,hit.Max,action,repeat});
    }
    if(pressed)action();
}
const char* CurrentLetter(const char* value);
void Character(const char* value, const Layout& l, float units = 1.0f) {
    ImGui::PushID(value);
    const bool block = std::strcmp(value, u8"█") == 0;
    if(block)ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0,0,0,0));
    ActionKey(value,Width(l,units),l.height,[value,block]{
        if(block){
            if(g_shortcuts_only){if(!QueueNative(chat_shortcut::kPayload,true,true))Limit();}
            else {keyboard_ui::SetText(chat_shortcut::kPayload);g_quick_send=true;}
        }
        else Insert(CurrentLetter(value));
        g_shift=false;
    });
    if(block){
        ImGui::PopStyleColor();
        const ImVec2 a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
        const float h=std::min(ImGui::GetFontSize(),(b.y-a.y)*.65f),w=h*.65f;
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2((a.x+b.x-w)*.5f,(a.y+b.y-h)*.5f),
            ImVec2((a.x+b.x+w)*.5f,(a.y+b.y+h)*.5f),ImGui::GetColorU32(ImGuiCol_Text));
    }
    ImGui::PopID();
}
void BeginShortcut(shortcut_editor::Kind kind) {
    InvalidateKeys();
    if (g_shortcut.Current() == kind) { CancelShortcut(); return; }
    if (g_shortcut.Begin(kind, g_text, g_cursor, g_anchor)) { g_page=Page::Letters; Dirty(); }
    else Limit();
}
void SwitchPage(Page page) {
    InvalidateKeys();
    if (g_shortcut.Active()) CancelShortcut();
    g_page = g_page == page ? Page::Letters : page;
    if (g_page == Page::Emoji && !g_emoji_ready) {
        if (g_integrated) g_emoji_request = true;
        else { g_emoji_ready=true; g_emoji_query_ok=true; g_emoji_mask=keyboard_catalog::kAllEmojiMask; }
    }
}
void RecordWindow() {
    if (g_rect_count >= 3) return;
    g_rect_min[g_rect_count]=ImGui::GetWindowPos();
    const auto size=ImGui::GetWindowSize();
    g_rect_max[g_rect_count]=ImVec2(g_rect_min[g_rect_count].x+size.x,g_rect_min[g_rect_count].y+size.y);
    ++g_rect_count;
}
int TextCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        const int limit = static_cast<int>(text_buffer::PrefixBytes(data->Buf,text_buffer::kLimit));
        if (data->BufTextLen > limit) data->DeleteChars(limit,data->BufTextLen-limit);
        data->CursorPos=std::min(data->CursorPos,data->BufTextLen);
        data->SelectionStart=std::min(data->SelectionStart,data->BufTextLen);
        data->SelectionEnd=std::min(data->SelectionEnd,data->BufTextLen);
    }
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && g_restore_cursor) {
        data->CursorPos=g_cursor; data->SelectionStart=g_anchor; data->SelectionEnd=g_cursor;
        g_restore_cursor=false;
    }
    if (!g_restore_cursor && !g_shortcut.Active()) {
        g_cursor=data->CursorPos;
        g_anchor=data->SelectionStart==data->SelectionEnd ? g_cursor :
            (data->SelectionStart==g_cursor ? data->SelectionEnd : data->SelectionStart);
    }
    return 0;
}
bool ComposerKeyboard() {
    if (!g_shortcut.Active()) return false;
    auto& io=ImGui::GetIO();
    if (g_shortcut.Current()==shortcut_editor::Kind::Size) {
        for (const ImWchar c:io.InputQueueCharacters) if (c>='0' && c<='9') {
            if (g_shortcut.SizeDigit(static_cast<char>(c),g_text,g_cursor,g_anchor)) Dirty(); else Limit();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && g_shortcut.SizeBackspace(g_text,g_cursor,g_anchor)) Dirty();
    }
    io.InputQueueCharacters.resize(0);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { CancelShortcut(); return true; }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        if (CommitSize()) Dirty();
        return true;
    }
    return false;
}
void Preview(bool composer_key) {
    const auto input_id=ImGui::GetID("##keyboard_text");
    auto* context=ImGui::GetCurrentContext();
    if (g_reset_history) {
        if (ImGui::GetActiveID()==input_id) ImGui::ClearActiveID();
        if (context->InputTextState.ID==input_id) context->InputTextState.ID=0;
        if (context->InputTextDeactivatedState.ID==input_id) context->InputTextDeactivatedState.ID=0;
        g_reset_history=false;
    }
    if (g_text_dirty) {
        if (context->InputTextDeactivatedState.ID==input_id) context->InputTextDeactivatedState.ID=0;
        if (auto* state=ImGui::GetInputTextState(input_id)) {
            state->ReloadUserBufAndKeepSelection();
            state->ReloadSelectionStart=g_anchor; state->ReloadSelectionEnd=g_cursor;
        }
    }
    // Stealing focus while a key is held cancels Delete repetition. Restore the
    // text cursor only after the gesture ends, including non-edit keys like Shift.
    if ((g_text_dirty || g_restore_cursor || g_refocus) && ImGui::GetActiveID()!=input_id && !ImGui::IsMouseDown(0))
        ImGui::SetKeyboardFocusHere();
    g_text_dirty=false;
    char count[24]; std::snprintf(count,sizeof(count),"%zu/1024",text_buffer::Count(g_text));
    const float counter_width=ImGui::CalcTextSize("1024/1024").x;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-counter_width-ImGui::GetStyle().ItemSpacing.x);
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackAlways|ImGuiInputTextFlags_CallbackEdit;
    if (g_password) flags|=ImGuiInputTextFlags_Password;
    if (g_shortcut.Active()) flags|=ImGuiInputTextFlags_ReadOnly;
    if (ImGui::InputText("##keyboard_text",g_text,sizeof(g_text),flags,TextCallback) && !composer_key && !g_shortcut.Active()) g_submit=true;
    if (ImGui::IsItemActive()) g_refocus=false;
    ImGui::SameLine();
    ImGui::TextColored(g_limit_until>ImGui::GetTime()?ImVec4(1,0.4f,0.3f,1):ImVec4(0.53f,0.65f,0.7f,1),"%s",count);
}
void Toolbar(Layout l) {
    if(g_shortcuts_only) {
        ActionKey("Boyut",Width(l,3),l.height,[]{BeginShortcut(shortcut_editor::Kind::Size);},g_shortcut.Current()==shortcut_editor::Kind::Size);
        ImGui::SameLine();
        ActionKey("Renk",Width(l,3),l.height,[]{BeginShortcut(shortcut_editor::Kind::Color);},g_shortcut.Current()==shortcut_editor::Kind::Color);
        ImGui::SameLine();Character("<",l,1.5f);
        ImGui::SameLine();Character(">",l,1.5f);
        ImGui::SameLine();Character(u8"█",l,3);
        return;
    }
    ActionKey("Boyut",Width(l,2.4f),l.height,[]{BeginShortcut(shortcut_editor::Kind::Size);},g_shortcut.Current()==shortcut_editor::Kind::Size);
    ImGui::SameLine();
    ActionKey("Renk",Width(l,2.4f),l.height,[]{BeginShortcut(shortcut_editor::Kind::Color);},g_shortcut.Current()==shortcut_editor::Kind::Color);
    ImGui::SameLine();
    ActionKey(g_page==Page::Emoji?"ABC123":"Emoji",Width(l,2.4f),l.height,[]{SwitchPage(Page::Emoji);},g_page==Page::Emoji);
    ImGui::SameLine(); Character("<",l,1.4f);
    ImGui::SameLine(); Character(">",l,1.4f);
    ImGui::SameLine(); Character(u8"█",l,2.0f);
}
void Footer(const Layout& l) {
    ImGui::PushID("footer");
    ActionKey(g_page==Page::Symbols?"ABC123":"#+=",Width(l,1.6f),l.height,[]{SwitchPage(Page::Symbols);},g_page==Page::Symbols);

    ImGui::SameLine(); ActionKey("Clear",Width(l,1.6f),l.height,[]{g_text[0]=0;g_cursor=g_anchor=0;Dirty();});
    ImGui::SameLine(); Character(",",l,0.6f);
    ImGui::SameLine(); ActionKey("Space",Width(l,3.0f),l.height,[]{Insert(" ");});
    ImGui::SameLine(); ActionKey(u8"Yapıştır",Width(l,1.4f),l.height,[]{Paste();});
    ImGui::SameLine(); Character(".",l,0.6f);
    ImGui::SameLine(); ActionKey("Enter",Width(l,1.9f),l.height,[]{g_submit=true;});
    ImGui::SameLine(); ActionKey("Gizle",Width(l,1.3f),l.height,[]{FinishShortcut();HideEditor();});
    ImGui::PopID();
}
struct Letter { const char* lower; const char* upper; };
const Letter kQ[]={{"q","Q"},{"w","W"},{"e","E"},{"r","R"},{"t","T"},{"y","Y"},{"u","U"},{u8"ı","I"},{"o","O"},{"p","P"},{u8"ğ",u8"Ğ"},{u8"ü",u8"Ü"}};
const Letter kA[]={{"a","A"},{"s","S"},{"d","D"},{"f","F"},{"g","G"},{"h","H"},{"j","J"},{"k","K"},{"l","L"},{u8"ş",u8"Ş"},{"i",u8"İ"}};
const Letter kZ[]={{"z","Z"},{"x","X"},{"c","C"},{"v","V"},{"b","B"},{"n","N"},{"m","M"},{u8"ö",u8"Ö"},{u8"ç",u8"Ç"}};
const char* CurrentLetter(const char* value){
    for(const auto* row:{kQ,kA,kZ}){
        const int count=row==kQ?12:row==kA?11:9;
        for(int i=0;i<count;++i)if(std::strcmp(value,row[i].lower)==0||std::strcmp(value,row[i].upper)==0)
            return Upper()?row[i].upper:row[i].lower;
    }
    return value;
}
void LetterKeys(const Letter* keys,int count,const Layout& l) {
    const bool upper=Upper();
    for (int i=0;i<count;++i) {
        if (i) ImGui::SameLine();
        Character(upper?keys[i].upper:keys[i].lower,l);
    }
}
const char* kSymbols[3][12]={
    {"|","\\","}","]","[","{",u8"½","$","#",u8"£","_","?"},
    {"=",")","(","/","&","%","+","^","'","<",">",u8"█"},
    {"!","@","\"","*","-",":",";","~","`",",",".",u8"€"}
};
void RowAt(float y,int row,const Layout& l) { ImGui::SetCursorPosY(y+row*(l.height+ImGui::GetStyle().ItemSpacing.y)); }
void Letters(const Layout& l,float y) {
    RowAt(y,0,l);
    const char* digits[]={"1","2","3","4","5","6","7","8","9","0","/","="};
    for (int i=0;i<12;++i) { if(i) ImGui::SameLine(); Character(digits[i],l); }
    RowAt(y,1,l); LetterKeys(kQ,12,l);
    RowAt(y,2,l); ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(l.unit+l.gap)*0.5f); LetterKeys(kA,11,l);
    RowAt(y,3,l);
    ActionKey("Shift",Width(l,1.5f),l.height,[]{
        if (g_last_shift>=0 && ImGui::GetTime()-g_last_shift<0.32) { g_caps=!g_caps; g_shift=false; g_last_shift=-1; }
        else { g_shift=!g_shift; g_last_shift=ImGui::GetTime(); }
    },g_shift||g_caps);
    ImGui::SameLine(); LetterKeys(kZ,9,l);
    ImGui::SameLine(); ActionKey("Delete",Width(l,1.5f),l.height,[]{Backspace();},false,true);
    RowAt(y,4,l); Footer(l);
}
void Symbols(const Layout& l,float y) {
    for(int row=0;row<3;++row) {
        RowAt(y,row,l);
        for(int i=0;i<12;++i) { if(i) ImGui::SameLine(); Character(kSymbols[row][i],l); }
    }
    RowAt(y,3,l); Footer(l);
}
void SizePad(const Layout& l,float y) {
    const char* digits[]={"1","2","3","4","5","6","7","8","9","0"};
    for(int row=0;row<2;++row) {
        RowAt(y,row,l);
        for(int i=0;i<5;++i) {
            if(i) ImGui::SameLine();
            ActionKey(digits[row*5+i],Width(l,2.4f),l.height,[digit=digits[row*5+i][0]]{
                if(g_shortcut.SizeDigit(digit,g_text,g_cursor,g_anchor)) Dirty(); else Limit();
            });
        }
    }
    RowAt(y,2,l);
    ActionKey("Delete",Width(l,3),l.height,[]{if(g_shortcut.SizeBackspace(g_text,g_cursor,g_anchor))Dirty();},false,true);
    ImGui::SameLine();
    ImGui::BeginDisabled(!g_shortcut.HasSize());
    ActionKey("Tamam",Width(l,3),l.height,[]{if(CommitSize())Dirty();InvalidateKeys();});
    ImGui::EndDisabled();
    ImGui::SameLine(); ActionKey(u8"Vazgeç",Width(l,3),l.height,[]{CancelShortcut();});
    ImGui::SameLine(); ActionKey("Gizle",Width(l,3),l.height,[]{FinishShortcut();HideEditor();});
}
float Hex(char c) { return static_cast<float>(c<='9'?c-'0':c-'A'+10)/15.0f; }
void Colors(const Layout& l,float y) {
    for(int row=0;row<2;++row) {
        RowAt(y,row,l);
        for(int i=0;i<5;++i) {
            if(i) ImGui::SameLine();
            const auto& c=keyboard_catalog::kColors[row*5+i];
            const ImVec4 color(Hex(c.rgba[0]),Hex(c.rgba[1]),Hex(c.rgba[2]),1);
            const auto linear=[](float v) { return v<=0.04045f?v/12.92f:std::pow((v+0.055f)/1.055f,2.4f); };
            const float luminance=linear(color.x)*0.2126f+linear(color.y)*0.7152f+linear(color.z)*0.0722f;
            ImGui::PushStyleColor(ImGuiCol_Button,color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(color.x*0.8f+0.2f,color.y*0.8f+0.2f,color.z*0.8f+0.2f,1));
            ImGui::PushStyleColor(ImGuiCol_Text,luminance>0.179f?ImVec4(0.02f,0.03f,0.04f,1):ImVec4(1,1,1,1));
            ActionKey(c.name,Width(l,2.4f),l.height,[rgba=c.rgba]{
                if(PickColor(rgba)) { g_page=Page::Letters; Dirty();InvalidateKeys(); } else Limit();
            });
            ImGui::PopStyleColor(3);
        }
    }
    RowAt(y,2,l);
    ActionKey(u8"Vazgeç",Width(l,6),l.height,[]{CancelShortcut();});
    ImGui::SameLine(); ActionKey("Gizle",Width(l,6),l.height,[]{CancelShortcut();HideEditor();});
}
void Emojis(const Layout& l,float y,float width) {
    std::vector<int> supported;
    for(size_t i=0;i<keyboard_catalog::kEmojiCount;++i) if(g_emoji_mask&(uint64_t{1}<<i)) supported.push_back(static_cast<int>(i));
    const int columns=width<600?4:6, per_page=columns*3-2;
    const int pages=std::max(1,(static_cast<int>(supported.size())+per_page-1)/per_page);
    g_emoji_page=std::clamp(g_emoji_page,0,pages-1);
    const float units=12.0f/columns;
    for(int row=0;row<3;++row) {
        RowAt(y,row,l);
        for(int col=0;col<columns;++col) {
            if(col) ImGui::SameLine();
            const int slot=row*columns+col;
            if(slot==per_page) {
                ImGui::BeginDisabled(g_emoji_page==0);
                ActionKey(u8"< Önce",Width(l,units),l.height,[]{--g_emoji_page;InvalidateKeys();});
                ImGui::EndDisabled();
            } else if(slot==per_page+1) {
                ImGui::BeginDisabled(g_emoji_page+1>=pages);
                ActionKey("Sonra >",Width(l,units),l.height,[]{++g_emoji_page;InvalidateKeys();});
                ImGui::EndDisabled();
            } else {
                const int index=g_emoji_page*per_page+slot;
                if(index<static_cast<int>(supported.size())) {
                    const auto& e=keyboard_catalog::kEmojis[supported[index]];
                    std::string label=e.name;
                    if(ImGui::GetFont()->FindGlyphNoFallback(static_cast<ImWchar>(e.codepoint))) label=e.utf8;
                    ImGui::PushID(supported[index]);
                    ActionKey(label.c_str(),Width(l,units),l.height,[value=e.utf8]{Insert(value);});
                    ImGui::PopID();
                } else ImGui::Dummy(ImVec2(Width(l,units),l.height));
            }
        }
    }
    if(supported.empty()) {
        ImGui::SetCursorPosY(y);
        if(!g_emoji_ready) ImGui::TextWrapped(u8"Oyunun emoji desteği kontrol ediliyor...");
        else if(!g_emoji_query_ok) ImGui::TextWrapped(u8"Bu alanın emoji desteği okunamadı.");
        else ImGui::TextWrapped(u8"Bu yazı tipinde desteklenen emoji bulunamadı.");
    }
    RowAt(y,3,l); Footer(l);
}
void DrawKeyboard(const ImGuiIO& io) {
    const auto& style=ImGui::GetStyle();
    const float width=io.DisplaySize.x*0.98f;
    const float header=ImGui::GetFrameHeight();
    const float fraction=io.DisplaySize.y<500?0.62f:(io.DisplaySize.y>io.DisplaySize.x?0.43f:0.52f);
    const float overhead=style.WindowPadding.y*2+header+style.ItemSpacing.y*6;
    const float key_height=std::max(12.0f,(io.DisplaySize.y*fraction-overhead)/6.0f);
    const int rows=g_shortcut.Active()?3:(g_page==Page::Letters?5:4);
    const float height=style.WindowPadding.y*2+header+(rows+1)*key_height+style.ItemSpacing.y*(rows+1);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x-width)*0.5f,io.DisplaySize.y-height),ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width,height),ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(.64f);
    // Keep text opaque and all key dimensions unchanged. Alpha affects only
    // backgrounds, rather than reducing readability with style.Alpha.
    const std::pair<ImGuiCol,float> alphas[]={{ImGuiCol_Button,.78f},{ImGuiCol_ButtonHovered,.88f},
        {ImGuiCol_ButtonActive,.95f},{ImGuiCol_FrameBg,.68f}};
    for(const auto& value:alphas){
        ImVec4 color=style.Colors[value.first];color.w=value.second;ImGui::PushStyleColor(value.first,color);
    }
    const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoScrollbar;
    if(!ImGui::Begin("##keyboard",nullptr,flags)) { ImGui::End(); ImGui::PopStyleColor(4);return; }
    RecordWindow();
    const float content=ImGui::GetContentRegionAvail().x;
    Layout l{std::max(1.0f,(content-style.ItemSpacing.x*11)/12.0f),style.ItemSpacing.x,key_height};
    ImGui::PushID("toolbar"); Toolbar(l); ImGui::PopID();
    const bool composer_key=ComposerKeyboard();
    Preview(composer_key);
    const float y=ImGui::GetCursorPosY();
    if(g_shortcut.Current()==shortcut_editor::Kind::Size) SizePad(l,y);
    else if(g_shortcut.Current()==shortcut_editor::Kind::Color) Colors(l,y);
    else if(g_page==Page::Symbols) Symbols(l,y);
    else if(g_page==Page::Emoji) Emojis(l,y,width);
    else Letters(l,y);
    ImGui::End();
    ImGui::PopStyleColor(4);
}
void DrawShortcutStrip(const ImGuiIO& io) {
    const auto state=native_shortcuts::Read();
    if(!state.active||state.session!=g_native_session)return;
    g_native_bottom=state.bottom;
    if(!g_shortcut.Active()) {
        const auto clean=text_buffer::Clean(state.text.c_str());
        std::memcpy(g_text,clean.c_str(),clean.size()+1);
        g_cursor=state.cursor;g_anchor=state.anchor;
    }
    if(state.limited)Limit();
    const auto& style=ImGui::GetStyle();
    const float width=io.DisplaySize.x*.98f;
    const float available=g_native_bottom>0?io.DisplaySize.y*g_native_bottom:io.DisplaySize.y*.45f;
    const bool expanded=g_shortcut.Active();
    const float gap=style.ItemSpacing.y;
    // Tiny landscape viewports retain touch-sized controls and scroll popup
    // contents instead of clipping controls behind the Android keyboard.
    const float key_height=std::clamp(io.DisplaySize.y*.055f,24.f,48.f);
    const int rows=g_strip_collapsed?1:(expanded?5:2);
    const float desired=2*style.WindowPadding.y+rows*key_height+(rows-1)*gap;
    const float height=std::min(desired,std::max(40.f,available-8.f));
    const float y=g_native_bottom>0?std::max(0.f,available-height-4.f):4.f;
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x-width)*.5f,y),ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width,height),ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(.82f);
    const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoFocusOnAppearing;
    if(!ImGui::Begin("##native_shortcuts",nullptr,flags)){ImGui::End();return;}
    RecordWindow();
    const float content=ImGui::GetContentRegionAvail().x;
    Layout l{std::max(1.f,(content-style.ItemSpacing.x*11)/12.f),style.ItemSpacing.x,key_height};
    if(g_strip_collapsed) {
        ActionKey(u8"Kısayollar",content,key_height,[]{g_strip_collapsed=false;InvalidateKeys();});
    } else {
        ImGui::BeginDisabled(g_password);
        ImGui::PushID("native_toolbar");Toolbar(l);ImGui::PopID();
        ImGui::EndDisabled();
        if(g_shortcut.Active()){
            ImGui::TextUnformatted(g_shortcut.Tag().c_str());
            const float body=ImGui::GetCursorPosY();
            if(g_shortcut.Current()==shortcut_editor::Kind::Size)SizePad(l,body);
            else if(g_shortcut.Current()==shortcut_editor::Kind::Color)Colors(l,body);

        } else {
            const float action_width=std::min(content*.3f,150.f);
            ImGui::BeginDisabled(g_password);
            ActionKey(u8"Yapıştır",action_width,key_height,[]{Paste();});
            ImGui::EndDisabled();ImGui::SameLine();
            ActionKey(u8"Küçült",action_width,key_height,[]{HideEditor();});ImGui::SameLine();
            ImGui::TextColored(g_limit_until>ImGui::GetTime()?ImVec4(1,.4f,.3f,1):ImVec4(.7f,.8f,.8f,1),
                               "%zu/1024",text_buffer::Count(state.text.c_str()));
        }
    }
    ImGui::End();
}
bool g_hub_open=false;
int g_hub_tab=0; // 0 = AI, 1 = Kıyafet, 2 = Oyun
// Oyun panelinde -1 bölüm listesini, 0..3 alt sayfaları gösterir.
int g_gameplay_page=-1;
void BotBody(const ImGuiIO& io,float key_height,float content,float max_height){
    const auto s=chat_bot_bridge::Read();
            ImGui::TextWrapped("%s",s.status.c_str());
            ImGui::TextWrapped("Model: %s | Anahtar: %s",s.model.c_str(),s.has_key?"var":"yok");
            {
                std::string who;
                if(s.all)who="herkes";
                else{for(const auto& n:s.selected){if(!who.empty())who+=", ";who+=n;}if(who.empty())who="(seçilmedi)";}
                ImGui::TextWrapped("Kişi: %s",who.c_str());
                ImGui::TextWrapped("Kişilik: %s",chat_bot_core::PersonaName(s.mode));
            }
            ImGui::BeginDisabled(!s.chat_ready);
            ImGui::PushID("bot_on");
            ActionKey(s.enabled?"Bot AÇIK - kapat":"Bot KAPALI - aç",content,key_height,[]{chat_bot_bridge::Toggle();InvalidateKeys();},s.enabled);
            ImGui::PopID();
            ImGui::PushID("bot_key");
            ActionKey("API anahtarı gir",content,key_height,[]{chat_bot_bridge::BeginEdit(chat_bot_bridge::Edit::Key);InvalidateKeys();});
            ImGui::PopID();
            ImGui::PushID("bot_persona");
            ActionKey("Özel kişilik yaz",content,key_height,[]{chat_bot_bridge::BeginEdit(chat_bot_bridge::Edit::Persona);InvalidateKeys();});
            ImGui::PopID();
            ImGui::EndDisabled();
            {   // Kişilik şablonları: Normal / Anı / İmam (+ yazılmışsa Özel)
                const float third=(content-2*ImGui::GetStyle().ItemSpacing.x)/3;
                for(int m=0;m<chat_bot_core::kPersonaCount;++m){
                    if(m)ImGui::SameLine();
                    ImGui::PushID(m+100);
                    ActionKey(chat_bot_core::PersonaName(m),third,key_height,[m]{chat_bot_bridge::SetMode(m);InvalidateKeys();},s.mode==m);
                    ImGui::PopID();
                }
                if(!s.persona.empty()){
                    ImGui::PushID("bot_mode_custom");
                    ActionKey("Özel kişiliği kullan",content,key_height,[]{chat_bot_bridge::SetMode(chat_bot_core::kPersonaCount);InvalidateKeys();},s.mode==chat_bot_core::kPersonaCount);
                    ImGui::PopID();
                }
                {   // Cevap uzunluğu sürgüsü: en az 45 kelime (şimdiki sınır), en çok 150
                    static int len_value=45;static bool len_active=false;
                    if(!len_active)len_value=s.words;
                    ImGui::SetNextItemWidth(content);
                    if(ImGui::SliderInt("##bot_len",&len_value,chat_bot_core::kMinWords,chat_bot_core::kMaxWords,"Cevap uzunluğu: %d kelime"))chat_bot_bridge::SetWords(len_value);
                    len_active=ImGui::IsItemActive();
                }
                ImGui::PushID("bot_strict");
                ActionKey(s.strict?"Ada göre süz: AÇIK (adım / yapay zeka)":"Ada göre süz: kapalı (hepsi)",content,key_height,[]{chat_bot_bridge::ToggleStrict();InvalidateKeys();},s.strict);
                ImGui::PopID();
                ImGui::PushID("bot_all");
                ActionKey(s.all?"Herkes AÇIK":"Herkes kapalı (seçili kişiler)",content,key_height,[]{chat_bot_bridge::ToggleAll();InvalidateKeys();},s.all);
                ImGui::PopID();
            }
            if(s.players.empty())ImGui::TextWrapped("Odada başka oyuncu yok / liste bekleniyor.");
            else{
                const float list_height=std::max(key_height,std::min(key_height*4,
                    max_height-ImGui::GetCursorPosY()-key_height*2-5*ImGui::GetStyle().ItemSpacing.y-2*ImGui::GetStyle().WindowPadding.y));
                ImGui::BeginChild("##bot_players",ImVec2(0,list_height),false);
                for(const auto& name:s.players){
                    ImGui::PushID(name.c_str());
                    ActionKey(name.c_str(),ImGui::GetContentRegionAvail().x,key_height,[name]{chat_bot_bridge::Select(name);InvalidateKeys();},std::find(s.selected.begin(),s.selected.end(),name)!=s.selected.end());
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
            ImGui::PushID("bot_refresh");
            ActionKey("Listeyi yenile",content,key_height,[]{chat_bot_bridge::Refresh();});
            ImGui::PopID();
            ImGui::PushID("bot_reset");
            ActionKey("Hafızayı sıfırla",content,key_height,[]{chat_bot_bridge::ResetMemory();});
            ImGui::PopID();
            if(!s.last_in.empty())ImGui::TextWrapped("Son satır: %s",s.last_in.c_str());
            if(!s.last_out.empty())ImGui::TextWrapped("Son cevap: %s",s.last_out.c_str());
}
void GameplaySectionBody(float key_height,float content){
    const auto s=gameplay_bridge::Read();
    const auto back=[]{g_gameplay_page=-1;InvalidateKeys();};
    ImGui::PushID("gameplay_back");
    ActionKey(u8"← Bölümlere dön",content,key_height,back);
    ImGui::PopID();
    ImGui::Spacing();ImGui::TextWrapped("%s",s.status.c_str());
    if(!s.ready)ImGui::TextWrapped("Odaya girip sahnenin yüklenmesini bekle.");
    const auto queue=[](gameplay_bridge::Command c){gameplay_bridge::Queue(c);InvalidateKeys();};
    const float half=(content-ImGui::GetStyle().ItemSpacing.x)/2;
    switch(g_gameplay_page){
    case 0:{
        static int selected=0;
        if(!s.items.empty()&&std::none_of(s.items.begin(),s.items.end(),[&](const auto& v){return v.id==selected;}))selected=s.items.front().id;
        ImGui::BeginDisabled(!s.spawn_ready);
        // Keep the action above the scrolling catalogue, including on short
        // landscape screens. An empty catalogue must not reserve four rows.
        ActionKey("Seçili eşyayı önüme oluştur",content,key_height,[queue,context=s.context,id=selected]{gameplay_bridge::Command c{gameplay_bridge::Kind::Spawn,context};c.item=id;queue(c);});
        if(!s.spawn_ready)ImGui::TextWrapped("%s",s.spawn_status.c_str());
        ImGui::TextWrapped("Oluşturulabilir eşya: %d",static_cast<int>(s.items.size()));
        if(!s.items.empty()){
            const float list_height=std::min(key_height*4,std::max(key_height,ImGui::GetContentRegionAvail().y-ImGui::GetStyle().ItemSpacing.y));
            if(ImGui::BeginChild("##game_items",ImVec2(0,list_height),false)){
                for(const auto& item:s.items){
                    ImGui::PushID(item.id);
                    ActionKey(item.name.c_str(),ImGui::GetContentRegionAvail().x,key_height,[id=item.id]{selected=id;InvalidateKeys();},item.id==selected);
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Eşya önüne bırakılır; ardından normal şekilde alabilirsin. İki oluşturma arasında bir saniye beklenir.");
        break;
    }
    case 1:{
        static size_t selected=0,previous=gameplay_bridge::kEnemies;
        static gameplay_bridge::Enemy edit;
        static bool editing=false;
        if(!s.authority)ImGui::TextWrapped("Düşman ayarları oda yöneticisinde kullanılabilir.");
        ImGui::BeginDisabled(!s.ready||!s.authority);
        for(size_t k=0;k<gameplay_bridge::kEnemies;++k){
            if(k%2)ImGui::SameLine();
            ImGui::PushID(static_cast<int>(k)+400);
            ActionKey(gameplay_bridge::kNames[k],half,key_height,[k]{selected=k;InvalidateKeys();},selected==k);
            ImGui::PopID();
        }
        if(!editing||previous!=selected)edit=s.enemies[selected];
        previous=selected;
        const auto send=[queue,context=s.context,enemy=selected](gameplay_bridge::Enemy value){gameplay_bridge::Command c{gameplay_bridge::Kind::EnemySettings,context};c.enemy=enemy;c.settings=value;queue(c);};
        ImGui::Text("Bulunan: %d",s.enemies[selected].count);
        ImGui::BeginDisabled(s.enemies[selected].count==0);
        ActionKey(edit.paused?"Devam et":"Durdur",content,key_height,[send,enemy=selected,value=edit]()mutable{value.paused=!value.paused;if(selected==enemy)edit=value;send(value);},edit.paused);
        editing=false;
        ImGui::BeginDisabled(selected==4);
        ImGui::SetNextItemWidth(content);
        if(ImGui::SliderFloat("##enemy_speed",&edit.speed,.1f,5.f,u8"Hız: %.1fx"))send(edit);
        editing|=ImGui::IsItemActive();ImGui::EndDisabled();
        ImGui::SetNextItemWidth(content);
        if(ImGui::SliderFloat("##enemy_animation",&edit.animation,.1f,3.f,u8"Animasyon: %.1fx"))send(edit);
        editing|=ImGui::IsItemActive();
        ImGui::SetNextItemWidth(content);
        if(ImGui::SliderFloat("##enemy_size",&edit.size,.25f,3.f,u8"Boyut: %.2fx"))send(edit);
        editing|=ImGui::IsItemActive();
        ActionKey("Bu düşmanın ayarlarını sıfırla",content,key_height,[queue,context=s.context,enemy=selected]{gameplay_bridge::Command c{gameplay_bridge::Kind::ResetEnemy,context};c.enemy=enemy;queue(c);editing=false;});
        ImGui::EndDisabled();ImGui::EndDisabled();
        if(selected>=4)ImGui::TextWrapped("Crow ve Rat ayarları yereldir; diğer oyuncularda aynı görünmeyebilir.");
        if(selected==4)ImGui::TextWrapped("Crow için ayrı hareket hızı alanı bulunmadı; animasyon hızını kullanabilirsin.");
        break;
    }
    case 2:
        ImGui::TextWrapped("Düşman nesnesini kaldırır. Normal ölüm animasyonu, süreli bayılma ve ödül akışı çalıştırılmaz.");
        if(!s.authority)ImGui::TextWrapped("Bu işlem için oda yöneticisi olmalısın.");
        ImGui::BeginDisabled(!s.ready||!s.authority);
        for(size_t k=0;k<gameplay_bridge::kEnemies;++k){
            if(k%2)ImGui::SameLine();
            ImGui::PushID(static_cast<int>(k)+500);ImGui::BeginDisabled(s.enemies[k].count==0);
            ActionKey(gameplay_bridge::kNames[k],half,key_height,[queue,context=s.context,k]{gameplay_bridge::Command c{gameplay_bridge::Kind::RemoveEnemy,context};c.enemy=k;queue(c);});
            ImGui::EndDisabled();ImGui::PopID();
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Crow ve Rat yalnız bu cihazda kaldırılır. Granny, Grandpa, MomSpider ve Spider için sahip olduğun ağ nesnesi kaldırılır.");
        break;
    case 3:
        ImGui::SeparatorText("Oyuncu dayanıklılığı");
        ImGui::BeginDisabled(!s.god_ready);
        ActionKey(s.god?"God Mode AÇIK - kapat":"God Mode KAPALI - aç",content,key_height,[queue,context=s.context,on=!s.god]{gameplay_bridge::Command c{gameplay_bridge::Kind::God,context};c.flag=on;queue(c);},s.god);
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s",s.god_status.c_str());
        ImGui::TextWrapped("Kendi karakterinin AI ve ölüm girişlerini engeller. Ölüm başladıktan sonra açmak karakteri yeniden canlandırmaz; tüm çevresel senaryoları kapsadığı doğrulanmadı.");
        break;
    default:break;
    }
}
void GameplayBody(float key_height,float content){
    if(g_gameplay_page>=0){
        static const char* titles[]={"Eşya spawn","Düşman durdur","Düşman öldür","God Mode"};
        if(g_gameplay_page<4)ImGui::Text("%s",titles[g_gameplay_page]);
        GameplaySectionBody(key_height,content);
        return;
    }
    ImGui::TextWrapped("Oyun özellikleri");
    ImGui::TextWrapped("Bir bölüme dokun. Geri dönünce tüm bölüm başlıkları görünür.");
    const char* labels[]={"Eşya spawn","Düşman durdur","Düşman öldür","God Mode"};
    const float half=(content-ImGui::GetStyle().ItemSpacing.x)/2;
    for(int i=0;i<4;++i){
        if(i%2)ImGui::SameLine();
        ImGui::PushID(i+300);
        ActionKey(labels[i],half,key_height,[i]{g_gameplay_page=i;InvalidateKeys();});
        ImGui::PopID();
    }
}
void WardrobeBody(const ImGuiIO& io,float key_height,float content){
    const auto s=wardrobe_bridge::Read();
            const float half=(content-ImGui::GetStyle().ItemSpacing.x)/2;
            ImGui::PushID("wd_tab0");ActionKey("Şapka",half,key_height,[]{g_wardrobe_tab=0;InvalidateKeys();},g_wardrobe_tab==0);ImGui::PopID();
            ImGui::SameLine();
            ImGui::PushID("wd_tab1");ActionKey("Karakter",half,key_height,[]{g_wardrobe_tab=1;InvalidateKeys();},g_wardrobe_tab==1);ImGui::PopID();
            const auto& list=s.items[g_wardrobe_tab];
            ImGui::TextWrapped("Gardıropta görünen: şapka %d, karakter %d",(int)s.items[0].size(),(int)s.items[1].size());
            if(!s.status.empty())ImGui::TextWrapped("%s",s.status.c_str());
            if(list.empty())ImGui::TextWrapped("Gardırop kart listesi henüz okunamadı. Oyun içi gardırop ekranını açıp tekrar kontrol et.");
            else{
                ImGui::TextWrapped("Oyunun gardırop kart listesinden okundu. Seçince oda görünümüne uygulanır.");
                ImGui::BeginChild("##wd_list",ImVec2(0,std::min(key_height*5.5f,io.DisplaySize.y*.4f)),false);
                if(g_wardrobe_tab==0){
                    ImGui::PushID("wd_none");
                    ActionKey("Şapkasız (çıkar)",ImGui::GetContentRegionAvail().x,key_height,[]{wardrobe_bridge::Select(0,std::string());InvalidateKeys();});
                    ImGui::PopID();
                }
                for(const auto& id:list){
                    ImGui::PushID(id.c_str());
                    const size_t tab=g_wardrobe_tab;
                    ActionKey(id.c_str(),ImGui::GetContentRegionAvail().x,key_height,[tab,id]{wardrobe_bridge::Select(tab,id);InvalidateKeys();},id==s.selected[tab]);
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
            ImGui::PushID("wd_rejoin");
            ActionKey("Çık-gir: herkese uygula",content,key_height,[]{wardrobe_bridge::RequestRejoin();InvalidateKeys();});
            ImGui::PopID();
            ImGui::TextWrapped("Diğer oyuncular kıyafeti yalnız odaya girişte okur. Seçimi yaptıktan sonra buna bas: odadan çıkıp aynı odaya yeniden girer.");
}
// "Game full" hatasından sonra yalnız o durumda görünen küçük düğme: oda boşalınca otomatik bağlan.
void DrawAutoJoin(const ImGuiIO& io){
    const auto s=auto_join_bridge::Read();
    if(!s.offer&&!s.active)return;
    const float scale=std::clamp(std::min(io.DisplaySize.x,io.DisplaySize.y)/500.0f,.7f,1.6f);
    const float width=std::min(io.DisplaySize.x*.7f,320*scale),key_height=40*scale;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.72f),ImGuiCond_Always,ImVec2(.5f,0));
    ImGui::SetNextWindowSize(ImVec2(width,0),ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(.8f);
    const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize;
    if(ImGui::Begin("##autojoin",nullptr,flags)){
        const float content=ImGui::GetContentRegionAvail().x;
        ImGui::PushID("autojoin");
        if(s.active){
            ImGui::TextWrapped("Oda: %s | deneme: %d",s.room.c_str(),s.attempts);
            ActionKey("Vazgeç",content,key_height,[]{auto_join_bridge::Cancel();InvalidateKeys();});
        }else{
            ActionKey("Otomatik bağlan (oda boşalınca)",content,key_height,[]{auto_join_bridge::Start();InvalidateKeys();});
        }
        ImGui::PopID();
        if(!s.status.empty())ImGui::TextWrapped("%s",s.status.c_str());
        RecordWindow();
    }
    ImGui::End();
}
// Yuvarlak, yarı saydam menü düğmesi + AI, Kıyafet ve bölümlemeli Oyun paneli.
void DrawHub(const ImGuiIO& io){
    const float scale=std::clamp(std::min(io.DisplaySize.x,io.DisplaySize.y)/500.0f,.7f,1.6f);
    const float key_height=36*scale,width=std::min(io.DisplaySize.x*.5f,340*scale);
    const float d=48*scale; // düğme çapı
    static ImVec2 pos(-1,-1);
    static float moved=0;
    if(pos.x<0)pos=ImVec2(8*scale,io.DisplaySize.y*.04f);
    pos.x=std::clamp(pos.x,0.0f,std::max(0.0f,io.DisplaySize.x-d));
    pos.y=std::clamp(pos.y,0.0f,std::max(0.0f,io.DisplaySize.y-d));
    ImGui::SetNextWindowPos(pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(d,d),ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,d);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0,0,0,0));
    const auto button_flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|
        ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoFocusOnAppearing;
    if(ImGui::Begin("##hubbtn",nullptr,button_flags)){
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0,0,0,0));
        ImGui::Button("##hub_btn",ImVec2(d,d));
        ImGui::PopStyleColor(3);
        if(ImGui::IsItemActivated())moved=0;
        if(ImGui::IsItemActive()){const ImVec2 delta=io.MouseDelta;moved+=std::fabs(delta.x)+std::fabs(delta.y);if(moved>8*scale)pos=ImVec2(pos.x+delta.x,pos.y+delta.y);}
        if(ImGui::IsItemDeactivated()&&moved<=8*scale){g_hub_open=!g_hub_open;InvalidateKeys();}
        const ImVec2 a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax(),c((a.x+b.x)*.5f,(a.y+b.y)*.5f);
        auto* draw=ImGui::GetWindowDrawList();
        const float alpha=ImGui::IsItemActive()?.62f:.42f;                 // şeffaf; basılıyken biraz belirgin
        draw->AddCircleFilled(c,d*.5f,ImGui::GetColorU32(ImVec4(.10f,.12f,.16f,alpha)),40);
        draw->AddCircle(c,d*.5f-1,ImGui::GetColorU32(ImVec4(1,1,1,alpha*.9f)),40,1.5f*scale);
        const float w=d*.30f,gap=d*.12f,t=std::max(2.0f,2.2f*scale);       // üç çizgi: menü simgesi
        for(int i=-1;i<=1;++i)draw->AddRectFilled(ImVec2(c.x-w,c.y+i*gap-t*.5f),ImVec2(c.x+w,c.y+i*gap+t*.5f),ImGui::GetColorU32(ImVec4(1,1,1,alpha+.35f)),t*.5f);
        RecordWindow();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if(!g_hub_open)return;
    const float panel_y=std::min(pos.y+d+6*scale,io.DisplaySize.y*.5f);
    const float max_height=std::max(80.0f*scale,io.DisplaySize.y-panel_y-4*scale);
    const float panel_x=std::clamp(pos.x,0.0f,std::max(0.0f,io.DisplaySize.x-width));
    ImGui::SetNextWindowPos(ImVec2(panel_x,panel_y),ImGuiCond_Always,ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(width,0),ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0,0),ImVec2(width,max_height));
    ImGui::SetNextWindowBgAlpha(.74f);
    const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize;
    if(ImGui::Begin("##hub",nullptr,flags)){
        const float content=ImGui::GetContentRegionAvail().x;
        const float half=(content-ImGui::GetStyle().ItemSpacing.x)/2;
        if(g_hub_tab==2&&g_gameplay_page>=0){
            // Subpage is focused: hide unrelated top-level tabs to keep the panel short.
            GameplayBody(key_height,content);
        }else{
            ImGui::PushID("hub_tab0");ActionKey("AI",half,key_height,[]{g_hub_tab=0;g_gameplay_page=-1;InvalidateKeys();},g_hub_tab==0);ImGui::PopID();
            ImGui::SameLine();
            ImGui::PushID("hub_tab1");ActionKey("Kıyafet",half,key_height,[]{g_hub_tab=1;g_gameplay_page=-1;InvalidateKeys();},g_hub_tab==1);ImGui::PopID();
            ImGui::PushID("hub_tab2");ActionKey("Oyun",content,key_height,[]{g_hub_tab=2;g_gameplay_page=-1;InvalidateKeys();},g_hub_tab==2);ImGui::PopID();
            if(g_hub_tab==0)BotBody(io,key_height,content,max_height);
            else if(g_hub_tab==1)WardrobeBody(io,key_height,content);
            else GameplayBody(key_height,content);
        }
        RecordWindow();
    }
    ImGui::End();
}
void DrawShowButton(const ImGuiIO& io) {
    const float margin=ImGui::GetStyle().WindowPadding.x;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x-margin,io.DisplaySize.y-margin),ImGuiCond_Always,ImVec2(1,1));
    if(ImGui::Begin("##keyboard_show",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings)) {
        if(ImGui::Button("Keyboard")) { g_visible=true; Dirty(); }
        RecordWindow();
    }
    ImGui::End();
}
}
namespace keyboard_ui {
void SetPasteProvider(PasteProvider provider) { g_paste=provider; }
void SetVisible(bool value) { if(value!=g_visible){g_rect_count=0;InvalidateKeys();}g_visible=value;if(value)Dirty(); }
void SetIntegrated(bool value) { g_integrated=value; }
void SetPassword(bool value) { g_password=value; }
void SetShortcutsOnly(bool value) {
    g_shortcuts_only=value;g_strip_collapsed=false;
    g_native_session=value?native_shortcuts::Read().session:0;
    InvalidateKeys();g_rect_count=0;
}
bool IsVisible() { return g_visible; }
void SetText(const char* text) {
    g_shortcut.Drop();
    const std::string clean=text_buffer::Clean(text);
    std::memcpy(g_text,clean.c_str(),clean.size()+1);
    g_cursor=g_anchor=static_cast<int>(clean.size()); Dirty();
}
bool ReconcileText(const char* expected, const char* actual) {
    if (g_shortcut.Active() || std::strcmp(g_text, expected) != 0) return false;
    SetText(actual); return true;
}
const char* GetText() { return g_text; }
const char* GetCommittedText() { return g_shortcut.Active()?g_shortcut.Before():g_text; }
int ConsumeSubmit() { const int result=g_submit?1:0; g_submit=false; return result; }
bool ConsumeQuickSend() { const bool result=g_quick_send;g_quick_send=false;return result; }
bool ConsumeEmojiRequest() { const bool result=g_emoji_request; g_emoji_request=false; return result; }
void SetEmojiSupport(uint64_t mask,bool query_ok) {
    g_emoji_mask=mask&keyboard_catalog::kAllEmojiMask; g_emoji_query_ok=query_ok;
    g_emoji_ready=true; g_emoji_page=0;
}
bool TouchDown(float x,float y){
    for(const auto& key:g_touch_keys)if(x>=key.min.x&&x<key.max.x&&y>=key.min.y&&y<key.max.y){
        const auto action=key.action;const bool repeat=key.repeat;
        g_repeat_min=key.min;g_repeat_max=key.max;g_touch_repeat=repeat?action:std::function<void()>{};
        g_repeat_at=ImGui::GetTime()+.22;g_refocus=true;action();return true;
    }
    return false;
}
void TouchHeld(float x,float y){
    if(!g_visible||!g_touch_repeat||x<g_repeat_min.x||x>=g_repeat_max.x||y<g_repeat_min.y||y>=g_repeat_max.y)return;
    if(ImGui::GetTime()>=g_repeat_at){const auto action=g_touch_repeat;g_repeat_at=ImGui::GetTime()+.035;action();}
}
void TouchUp(){g_touch_repeat={};}
bool HitTest(float x,float y) {
    for(int i=0;i<g_rect_count;++i) if(x>=g_rect_min[i].x&&x<g_rect_max[i].x&&y>=g_rect_min[i].y&&y<g_rect_max[i].y) return true;
    return false;
}
void ResetTransientState() {
    InvalidateKeys();
    if(g_shortcut.Active()) { g_shortcut.Cancel(g_text,g_cursor,g_anchor); Dirty(); }
    g_reset_history=true; g_shift=g_caps=g_submit=g_refocus=false;
    g_quick_send=false;
    g_page=Page::Letters; g_last_shift=-1; g_limit_until=-1; g_rect_count=0;
    g_emoji_mask=0; g_emoji_ready=g_emoji_query_ok=g_emoji_request=false; g_emoji_page=0;
}
void Draw() {
    g_touch_keys.clear();
    g_rect_count=0;
    const auto& io=ImGui::GetIO();
    if(io.DisplaySize.x<=0||io.DisplaySize.y<=0) return;
    if(g_visible) {if(g_shortcuts_only)DrawShortcutStrip(io);else DrawKeyboard(io);}
    else if(g_integrated){DrawHub(io);DrawAutoJoin(io);}
    else DrawShowButton(io);
}
}
