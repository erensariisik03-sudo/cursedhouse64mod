#include "keyboard_ui.h"
#include "text_buffer.h"
#include "shortcut_editor.h"
#include "native_shortcuts.h"
#include "keyboard_catalog.h"
#include "chat_shortcut.h"
#include "lobby_emote_bridge.h"
#include "chat_bot_bridge.h"
#include "photo_transfer_bridge.h"
#include "photo_transport_bridge.h"
#include "storage_access.h"
#include "voice_ai_bridge.h"
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
#include <sstream>

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
struct TouchKey { ImVec2 min,max;std::function<void()> action;bool repeat;bool list=false; };
std::vector<TouchKey> g_touch_keys;
std::function<void()> g_touch_repeat;
ImVec2 g_repeat_min{},g_repeat_max{};
double g_repeat_at=0;
size_t g_wardrobe_tab=0;
keyboard_ui::PasteProvider g_paste=nullptr;
keyboard_ui::CopyProvider g_copy=nullptr;
struct Layout { float unit, gap, height; };
struct ListTouch {
    ImVec2 min{},max{};ImGuiID window=0;uint64_t context=0;
    bool visible=false,down=false,dragged=false,recording=false;
    float start_x=0,start_y=0,last_y=0,delta=0,threshold=8;
    std::function<void()> tap;
    bool thumb_down=false;
    float track_top=0,track_bottom=0,thumb_height=0,scroll_max=0,scroll_set=-1;
    ImVec2 bar_min{},bar_max{};
};
ListTouch g_list_touch,g_hub_touch;
// ImGui sliders must receive the touch themselves; otherwise the hub/list scroll handler swallows the drag.
std::vector<std::pair<ImVec2,ImVec2>> g_slider_rects;
void TrackSlider(){g_slider_rects.emplace_back(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());}
bool g_touching_hub=false;
void InvalidateKeys(){g_touch_keys.clear();g_touch_repeat={};g_list_touch={};g_hub_touch={};}

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
        if(hit.Min.x<hit.Max.x&&hit.Min.y<hit.Max.y)g_touch_keys.push_back({hit.Min,hit.Max,action,repeat,g_list_touch.recording||g_hub_touch.recording});
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
int g_bot_page=-1;
// One touch-aware scroll region is visible at a time on each AI subpage.
void RenderTouchThumb(ListTouch& t,ImGuiWindow* window){
    t.scroll_max=ImGui::GetScrollMaxY();
    const ImVec2 a=window->InnerClipRect.Min,b=window->InnerClipRect.Max;
    t.track_top=a.y+3;t.track_bottom=b.y-3;
    const float track_h=std::max(1.f,t.track_bottom-t.track_top);
    t.thumb_height=t.scroll_max>1?std::max(22.f,track_h*track_h/(track_h+t.scroll_max)):track_h;
    t.thumb_height=std::min(t.thumb_height,track_h);
    const float bar_x=std::min(window->Pos.x+window->Size.x-11.f,b.x+2.f);
    t.bar_min=ImVec2(bar_x-5.f,t.track_top);t.bar_max=ImVec2(bar_x+10.f,t.track_bottom);
    if(t.scroll_max>1){
        const float y=t.track_top+(track_h-t.thumb_height)*std::clamp(ImGui::GetScrollY()/t.scroll_max,0.f,1.f);
        window->DrawList->AddRectFilled(ImVec2(bar_x,y),ImVec2(bar_x+5.f,y+t.thumb_height),IM_COL32(95,195,210,220),3.f);
    }
}
void UpdateTouchScroll(ListTouch& t){
    if(t.scroll_set>=0){ImGui::SetScrollY(std::clamp(t.scroll_set,0.f,ImGui::GetScrollMaxY()));t.scroll_set=-1;}
    if(t.delta!=0){ImGui::SetScrollY(std::clamp(ImGui::GetScrollY()+t.delta,0.f,ImGui::GetScrollMaxY()));t.delta=0;}
}
void BeginTouchList(const char* id,float height){
    ImGui::BeginChild(id,ImVec2(0,height),false,ImGuiWindowFlags_AlwaysVerticalScrollbar);
    auto* window=ImGui::GetCurrentWindow();
    if(g_list_touch.window!=window->ID)g_list_touch={};
    auto& t=g_list_touch;t.window=window->ID;t.visible=true;t.recording=true;
    t.min=window->InnerClipRect.Min;t.max=window->InnerClipRect.Max;
    t.threshold=std::max(7.f,ImGui::GetFrameHeight()*.23f);
    UpdateTouchScroll(t);
}
void EndTouchList(){
    auto* window=ImGui::GetCurrentWindow();auto& t=g_list_touch;t.recording=false;
    RenderTouchThumb(t,window);
    ImGui::EndChild();
}
// 0: Metin AI, 1: Sesli AI, 2: Kişilik, 3: API, 4: Temsil, 5: Geçmiş, 6: Kelimeler, 7: Konuşmacı, 8: Yanıt türü
void BotBody(const ImGuiIO&,float key_height,float content,float max_height){
    const auto s=chat_bot_bridge::Read();const auto v=voice_ai_bridge::Read();
    // A selected SEDA utterance should be visible immediately. Capture stays
    // paused until the user submits/ignores it, so never silently auto-answer.
    static bool had_pending=false;
    const bool pending=!v.selection_text.empty()&&v.settings.manual_keywords;
    if(pending&&!had_pending&&g_bot_page==1)g_bot_page=6;
    had_pending=pending;
    // A stale open page must not survive a developer-session lock.
    if(g_bot_page==10&&!s.developer_unlocked)g_bot_page=9;
    static int previous=-2;if(previous!=g_bot_page){ImGui::SetScrollY(0);previous=g_bot_page;}
    const float half=(content-ImGui::GetStyle().ItemSpacing.x)/2;
    if(g_bot_page<0){
        const float menu_height=std::max(19.f,key_height*.70f);
        ActionKey(s.cool?"Cool Modu: AÇIK":"Cool Modu: KAPALI",content,menu_height,[]{chat_bot_bridge::ToggleCool();InvalidateKeys();},s.cool);
        const char* labels[]={"API Anahtarı","Metin AI","Sesli AI","Kişilik","Oyuncu mesajı","Geçmiş","Geliştirici"};
        const int pages[]={3,0,1,2,4,5,9};
        for(int i=0;i<7;++i){if(i%2)ImGui::SameLine();ImGui::PushID(i+500);
            ActionKey(labels[i],half,menu_height,[page=pages[i]]{g_bot_page=page;InvalidateKeys();});ImGui::PopID();}
        ImGui::TextWrapped("%s",chat_bot_core::PrefixScalars(s.status,65).c_str());return;
    }
    ActionKey((g_bot_page==6||g_bot_page==7||g_bot_page==8)?u8"< Sesli AI":(g_bot_page==10?u8"< Geliştirici":u8"< Genel AI menüsü"),content,key_height,[]{g_bot_page=(g_bot_page==6||g_bot_page==7||g_bot_page==8)?1:(g_bot_page==10?9:-1);InvalidateKeys();});
    switch(g_bot_page){
    case 9:{
        if(!s.developer_unlocked){
            ImGui::TextWrapped("Geliştirici erişimi kilitli.");
            ImGui::TextWrapped("Kalan deneme: %d", s.developer_attempts_left);
            ImGui::BeginDisabled(s.developer_attempts_left<=0);
            ActionKey("Şifreyi gir",content,key_height,[]{chat_bot_bridge::BeginEdit(chat_bot_bridge::Edit::DeveloperUnlock);InvalidateKeys();});
            ImGui::EndDisabled();
            break;
        }
        ImGui::TextWrapped("Geliştirici");
        ActionKey("Geliştirici kilidini kapat",content,key_height,[]{chat_bot_bridge::LockDeveloper();InvalidateKeys();});
        // Photo requests and receiving belong behind developer authentication.
        // The remote sender can still accept a single explicit share request
        // from the top-level consent popup without entering the password.
        ImGui::Separator();
        ImGui::TextWrapped("Diger oyuncunun paylasima actigi fotograflar icin GMM Connect'i ac.");
        ActionKey("GMM Connect'e git",content,key_height,[]{g_bot_page=10;InvalidateKeys();});
        break;
    }
    case 10:{
        if(!s.developer_unlocked){g_bot_page=9;break;}
        const auto net=photo_transport_bridge::Read();
        ImGui::TextWrapped("GMM Connect | Karsi cihazdan fotograf iste");
        ImGui::TextWrapped("%s",net.status.c_str());
        ImGui::TextWrapped("Tani: hook=%d | oda=%d | aktor=%d | oyuncu=%d | secilen=%d | istek=%d",
            net.ready,net.in_room,net.local_actor,(int)net.peers.size(),net.selected_actor,net.browsing);
        if(!net.ready)ImGui::TextWrapped("Photon dosya kanali acilamadi. 'adb logcat -d -s multiplayermod:I' ile GMM photo resolve kayitlarini kontrol et.");
        else if(!net.in_room)ImGui::TextWrapped("Photon kanali hazir ama henuz oda yok. Ayni odaya girip tekrar dene.");
        else if(net.peers.empty())ImGui::TextWrapped("Oda var, fakat Photon PlayerListOthers bos. Iki cihaz ayni Photon odasinda mi?");
        ActionKey("Oyuncu listesini yenile",content,key_height,[]{photo_transport_bridge::Refresh();InvalidateKeys();});
        if(net.peers.empty())ImGui::TextWrapped("Ayni odada baska oyuncu bulunamadi. Oyunculari yenile ve baglanti durumunu kontrol et.");
        else {
            ImGui::TextWrapped("Hedef oyuncu sec (listeden birine dokun):");
            BeginTouchList("##gmm_photo_peers_developer",std::max(key_height,key_height*2.5f));
            for(const auto& peer:net.peers){
                ImGui::PushID(peer.actor);
                const std::string label="#"+std::to_string(peer.actor)+" "+peer.display;
                ActionKey(label.c_str(),ImGui::GetContentRegionAvail().x,key_height,[id=peer.actor]{photo_transport_bridge::ChooseActor(id);InvalidateKeys();},net.selected_actor==peer.actor);
                ImGui::PopID();
            }
            EndTouchList();
        }
        if(net.selected_actor>0)ImGui::TextWrapped("Secilen hedef: %s",net.selected_name.c_str());
        else ImGui::TextWrapped("Once yukaridaki bir oyuncunun SATIRINA dokun. Baslik tiklanabilir degil.");
        if(net.browsing)ImGui::TextWrapped("Onceki liste istegi devam ediyor. Istege yanit veya zaman asimi bekleniyor.");
        ImGui::BeginDisabled(!net.ready||!net.in_room||net.selected_actor<=0||net.browsing);
        ActionKey("Hedef oyuncudan fotograflari iste",content,key_height,[]{photo_transport_bridge::RequestBrowse();InvalidateKeys();});
        ImGui::EndDisabled();
        if(net.browsing){
            if(!net.fetching)ActionKey("Fotograf listesi istegini iptal et",content,key_height,[]{photo_transport_bridge::CancelBrowse();InvalidateKeys();});
            ImGui::TextWrapped(net.listing_ready?"Oyuncunun izin verdigi fotograflar:":"Karsi oyuncunun onayi / listesi bekleniyor...");
            if(net.listing_ready){
                ImGui::TextWrapped("Karsidan gelen fotograf sayisi: %d",(int)net.remote_files.size());
                if(net.remote_files.size()==1)
                    ImGui::TextWrapped("Tek fotograf varsa otomatik secilir. Asagidaki Al butonuna basabilirsin.");
                else
                    ImGui::TextWrapped("Fotografi indirmek icin dosyanin 'Al:' satirina dokun.");
                // Do not put the remote file buttons in a second scrollable
                // child. On real devices the nested touch region can intercept
                // their taps; the parent hub already supports touch scrolling.
                for(const auto& name:net.remote_files){
                    ImGui::PushID(name.c_str());
                    const std::string label="Al: "+name;
                    ActionKey(label.c_str(),ImGui::GetContentRegionAvail().x,key_height,[name]{photo_transport_bridge::FetchRemoteFile(name);InvalidateKeys();},net.selected_remote_file==name);
                    ImGui::PopID();
                }
                if(net.remote_files.empty())ImGui::TextWrapped("Paylasilan klasorde JPG/PNG yok.");
                if(!net.selected_remote_file.empty())ImGui::TextWrapped("Secilen dosya: %s",net.selected_remote_file.c_str());
                else if(net.remote_files.size()>1)ImGui::TextWrapped("Henuz bir dosya secilmedi.");
                ImGui::BeginDisabled(net.fetching||net.remote_files.empty());
                ActionKey("Secilen fotografi al",content,key_height,[]{photo_transport_bridge::Fetch();InvalidateKeys();});
                ImGui::EndDisabled();
            }
        }
        ImGui::TextWrapped("Karsi cihaz kabul etmeden dosya adi gelmez. Yalnizca karsi cihazdaki Pictures/GMMShare gosterilir.");
        break;
    }
    case 0:{
        ImGui::TextWrapped("METİN AI - diğer AI türü açıksa otomatik kapatılır.");
        ImGui::BeginDisabled(!s.chat_ready);
        ActionKey(s.enabled?"Metin AI: AÇIK - kapat":"Metin AI: KAPALI - aç",content,key_height,[enabled=!s.enabled]{
            if(enabled){voice_ai_bridge::Enable(false);chat_bot_bridge::VoiceFx(false);}
            chat_bot_bridge::Enable(enabled);InvalidateKeys();},s.enabled);
        ImGui::EndDisabled();
        ActionKey(s.all?"Herkes: AÇIK":"Herkes: KAPALI",content,key_height,[]{chat_bot_bridge::ToggleAll();InvalidateKeys();},s.all);
        float list_height=std::max(key_height,std::min(key_height*3,max_height-ImGui::GetCursorPosY()-key_height*3));
        BeginTouchList("##bot_players",list_height);
        if(s.players.empty())ImGui::TextWrapped("Oda oyuncuları bekleniyor.");
        for(const auto& name:s.players){ImGui::PushID(name.c_str());ActionKey(name.c_str(),ImGui::GetContentRegionAvail().x,key_height,[name]{chat_bot_bridge::Select(name);InvalidateKeys();},std::find(s.selected.begin(),s.selected.end(),name)!=s.selected.end());ImGui::PopID();}
        EndTouchList();
        ActionKey(s.message_filter?"Mesaj filtresi: AÇIK":"Mesaj filtresi: KAPALI",content,key_height,[]{chat_bot_bridge::ToggleFilter();InvalidateKeys();},s.message_filter);
        ActionKey(s.strict?"Adla çağırma: AÇIK":"Adla çağırma: KAPALI",content,key_height,[]{chat_bot_bridge::ToggleStrict();InvalidateKeys();},s.strict);
        int words=s.words;ImGui::SetNextItemWidth(content);if(ImGui::SliderInt("##bot_len",&words,chat_bot_core::kMinWords,chat_bot_core::kMaxWords,"Yanıt: %d kelime"))chat_bot_bridge::SetWords(words);TrackSlider();
        ActionKey("Sohbet hafızasını sıfırla",content,key_height,[]{chat_bot_bridge::ResetMemory();InvalidateKeys();});break;
    }
    case 1:{
        ImGui::TextWrapped("SESLİ AI | Aynı anda yalnızca bir konuşmacı dinlenir.");
        ImGui::TextWrapped("%s",v.status.c_str());
        ImGui::BeginDisabled(!v.ready||!s.has_key);
        ActionKey(v.settings.enabled?"Sesli AI: AÇIK - kapat":"Sesli AI: KAPALI - aç",content,key_height,[enabled=!v.settings.enabled]{
            if(enabled)chat_bot_bridge::Enable(false);
            voice_ai_bridge::Enable(enabled);chat_bot_bridge::VoiceFx(enabled);InvalidateKeys();},v.settings.enabled);
        ImGui::EndDisabled();
        ActionKey(v.selection_text.empty()?"Konuşma kelimelerini seç (bekleniyor)":"Konuşma kelimelerini seç",content,key_height,[]{g_bot_page=6;InvalidateKeys();},!v.selection_text.empty());
        ActionKey("Konuşmacı seç",content,key_height,[]{g_bot_page=7;InvalidateKeys();});
        ActionKey("Konuşmayı durdur",content,key_height,[]{voice_ai_bridge::Stop();InvalidateKeys();});
        ActionKey("Yanıt türünü seç",content,key_height,[]{g_bot_page=8;InvalidateKeys();});
        ActionKey(v.settings.manual_keywords?"Manuel kelime seçimi: AÇIK":"Manuel kelime seçimi: KAPALI",content,key_height,[enabled=!v.settings.manual_keywords]{voice_ai_bridge::ManualKeywords(enabled);InvalidateKeys();},v.settings.manual_keywords);
        {int tone=voice_ai_bridge::tone.load();ImGui::SetNextItemWidth(content);if(ImGui::SliderInt("##voice_tone",&tone,0,100,"Ses tonu: %d%%"))voice_ai_bridge::SetTone(tone);TrackSlider();}
        ActionKey("Kendi mesajımı yaz",content,key_height,[]{chat_bot_bridge::BeginEdit(chat_bot_bridge::Edit::Speech);InvalidateKeys();});
        const auto saved=voice_ai_bridge::SpeechText();if(!saved.empty()){
            ImGui::TextWrapped("Metin: %s",chat_bot_core::PrefixScalars(saved,110).c_str());
            ImGui::BeginDisabled(!v.settings.enabled);ActionKey("Mesajı seslendir",content,key_height,[saved]{voice_ai_bridge::Speak(saved);InvalidateKeys();});ImGui::EndDisabled();
        }
        ImGui::TextWrapped("Sesli AI yanıtı aynı zamanda sohbete yazılır. İsim filtresi Sesli AI için kapalıdır.");break;
    }
    case 2:{
        ImGui::TextWrapped("ORTAK KİŞİLİK | Sesli AI ve Metin AI aynı ayarı kullanır.");
        for(int mode=0;mode<chat_bot_core::kPersonaCount;++mode)ActionKey(chat_bot_core::PersonaName(mode),content,key_height,[mode]{chat_bot_bridge::SetMode(mode);InvalidateKeys();},s.mode==mode);
        ActionKey("Özel kişilik yaz",content,key_height,[]{chat_bot_bridge::BeginEdit(chat_bot_bridge::Edit::Persona);InvalidateKeys();});
        if(!s.persona.empty())ActionKey("Özel kişiliği kullan",content,key_height,[]{chat_bot_bridge::SetMode(chat_bot_core::kPersonaCount);InvalidateKeys();},s.mode==chat_bot_core::kPersonaCount);break;
    }
    case 3:
        ImGui::TextWrapped("API Anahtarı | %s",s.has_key?"Tanımlı":"Henüz tanımlı değil");
        ActionKey("API anahtarı gir / değiştir",content,key_height,[]{chat_bot_bridge::BeginEdit(chat_bot_bridge::Edit::Key);InvalidateKeys();});break;
    case 4:{
        ImGui::TextWrapped("Oyuncu adını seçip mesaj yaz. Sohbet görünür adı seçili oyuncu adı olabilir, fakat Photon aktör kimliğin kendi hesabın olarak kalır.");
        if(s.delegate_players.empty())ImGui::TextWrapped("Önce odaya gir; oyuncu listesi bekleniyor.");
        BeginTouchList("##delegate_players",key_height*3);
        {
            for(const auto& name:s.delegate_players){
                ImGui::PushID(name.c_str());
                // ImGui treats "##" as an ID marker; draw the nickname as raw
                // text instead so symbols/emoji are displayed without loss.
                ActionKey("##delegate_nickname",ImGui::GetContentRegionAvail().x,key_height,[name]{chat_bot_bridge::SelectDelegate(name);InvalidateKeys();},s.delegate_name==name);
                const ImVec2 min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
                const ImVec2 text_pos(min.x+8.f,min.y+(key_height-ImGui::GetFontSize())*.5f);
                auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(ImVec2(min.x+6.f,min.y),ImVec2(max.x-5.f,max.y),true);
                draw->AddText(text_pos,ImGui::GetColorU32(ImGuiCol_Text),name.c_str());
                draw->PopClipRect();
                ImGui::PopID();
            }
        }EndTouchList();
        ImGui::TextWrapped("Seçilen: %s",s.delegate_name.empty()?"Yok":s.delegate_name.c_str());
        ImGui::BeginDisabled(s.delegate_name.empty());
        ActionKey("Mesaj yaz ve gönder",content,key_height,[]{chat_bot_bridge::BeginEdit(chat_bot_bridge::Edit::Delegate);InvalidateKeys();});
        ImGui::EndDisabled();break;
    }
    case 8:{
        ImGui::TextWrapped("Yanıt biçimi | Üç moddan yalnızca biri aktiftir.");
        ActionKey("Metni sesle cevapla",content,key_height,[]{voice_ai_bridge::Mode(1,1);InvalidateKeys();},v.settings.input==1&&v.settings.output==1);
        ActionKey("Sesi sesle cevapla",content,key_height,[]{voice_ai_bridge::Mode(0,1);InvalidateKeys();},v.settings.input==0&&v.settings.output==1);
        ActionKey("Metin + sesi sesle cevapla",content,key_height,[]{voice_ai_bridge::Mode(2,1);InvalidateKeys();},v.settings.input==2&&v.settings.output==1);
        ImGui::TextWrapped("Sesli AI yanıtları ayrıca oyun sohbetine yazılır.");break;
    }
    case 7:{
        ImGui::TextWrapped("SEDA | Bir anda bir oyuncu dinlenir.");
        ActionKey("Otomatik tek konuşmacı",content,key_height,[]{voice_ai_bridge::TargetActor(0);InvalidateKeys();},v.settings.target_actor==0);
        if(v.players.empty())ImGui::TextWrapped("Henüz sesli sohbete katılan oyuncu yok.");
        BeginTouchList("##pick_voice_players",std::max(key_height*2,max_height-ImGui::GetCursorPosY()-key_height*1.5f));
        {
            for(const auto& player:v.players){ImGui::PushID(player.first);
                std::string label=player.second+(v.active_actor==player.first?" (dinleniyor)":"");
                ActionKey(label.c_str(),ImGui::GetContentRegionAvail().x,key_height,[actor=player.first]{voice_ai_bridge::TargetActor(actor);InvalidateKeys();},v.settings.target_actor==player.first);
                ImGui::PopID();}
        }EndTouchList();break;
    }
    case 6:{
        ImGui::TextWrapped("SEDA | Kelime seçimi (sadece seçilenler AI'ye gider)");
        if(!v.selection_text.empty()){
            static std::string seen;static std::vector<bool> chosen;
            std::vector<std::string> tokens;std::istringstream split(v.selection_text);std::string word;
            while(split>>word&&tokens.size()<120)tokens.push_back(word);
            if(seen!=v.selection_name+"/"+v.selection_text){seen=v.selection_name+"/"+v.selection_text;chosen.assign(tokens.size(),false);}
            ImGui::TextWrapped("%s konuştu - anahtar kelimeleri seç:",v.selection_name.c_str());
            const float list_height=std::max(60.f,std::min(key_height*3,max_height-ImGui::GetCursorPosY()-key_height*4));
            BeginTouchList("##selection_words",list_height);
            for(size_t i=0;i<tokens.size();++i){ImGui::PushID(int(i));
                if(i%2)ImGui::SameLine();
                ActionKey(tokens[i].c_str(),half,key_height,[i]{if(i<chosen.size())chosen[i]=!chosen[i];InvalidateKeys();},i<chosen.size()&&chosen[i]);
                ImGui::PopID();}
            EndTouchList();
            ActionKey("Hepsini seç",half,key_height,[count=tokens.size()]{chosen.assign(count,true);InvalidateKeys();});ImGui::SameLine();
            ActionKey("Seçimi sil",half,key_height,[count=tokens.size()]{chosen.assign(count,false);InvalidateKeys();});
            const auto selected_count=std::count(chosen.begin(),chosen.end(),true);
            ImGui::Text("Seçilen: %d / %d",int(selected_count),int(tokens.size()));
            ImGui::BeginDisabled(selected_count==0);
            ActionKey("Seçilenleri AI'ye gönder",content,key_height,[]{std::vector<int> indices;for(size_t i=0;i<chosen.size();++i)if(chosen[i])indices.push_back(int(i));if(!indices.empty())voice_ai_bridge::SubmitSelected(std::move(indices));InvalidateKeys();});
            ImGui::EndDisabled();
            ActionKey("Bu konuşmayı atla",content,key_height,[]{voice_ai_bridge::SkipSelected();InvalidateKeys();});
        }
        else ImGui::TextWrapped("Bekleyen konuşma yok. Sesli AI ve Manuel kelime seçimi açıkken bir oyuncu konuşunca kelimeler burada çıkacak. Dinlenen kişi: %s",v.active_actor?"seçili oyuncu":"henüz seçilmedi");
        break;
    }
    default:
        ImGui::TextWrapped("Son mesaj: %s",s.last_in.c_str());
        ImGui::TextWrapped("Son AI cevabı: %s",s.last_out.c_str());
        ImGui::TextWrapped("Son konuşma: %s",v.heard.c_str());
        ImGui::TextWrapped("Sesli AI cevabı: %s",v.reply.c_str());break;
    }
}

void GameplaySectionBody(float key_height,float content){
    const auto s=gameplay_bridge::Read();
    const auto back=[]{g_gameplay_page=-1;InvalidateKeys();};
    ImGui::PushID("gameplay_back");
    ActionKey(u8"< Bölümlere dön",content,key_height,back);
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
        const auto selection=std::find_if(s.items.begin(),s.items.end(),[&](const auto& v){return v.id==selected;});
        const bool available=selection!=s.items.end()&&selection->available;
        ImGui::BeginDisabled(!available);
        ActionKey("Seçili eşyayı oluştur",content,key_height,[queue,context=s.context,id=selected]{gameplay_bridge::Command c{gameplay_bridge::Kind::Spawn,context};c.item=id;queue(c);});
        ImGui::EndDisabled();
        if(!s.spawn_ready)ImGui::TextWrapped("%s",s.spawn_status.c_str());
        ImGui::TextWrapped("Oluşturulabilir eşya: %d",static_cast<int>(std::count_if(s.items.begin(),s.items.end(),[](const auto& item){return item.available;})));
        if(!s.items.empty()){
            const float list_height=std::min(key_height*4,std::max(key_height,ImGui::GetContentRegionAvail().y-ImGui::GetStyle().ItemSpacing.y));
            if(ImGui::BeginChild("##game_items",ImVec2(0,list_height),false)){
                auto* list=ImGui::GetCurrentWindow();
                if(g_list_touch.window!=list->ID||g_list_touch.context!=s.context)g_list_touch={};
                g_list_touch.window=list->ID;g_list_touch.context=s.context;g_list_touch.visible=true;
                // ClipRect is a stack value and changes when EndChild pops it.
                // InnerClipRect retains the actual visible list content area.
                g_list_touch.min=list->InnerClipRect.Min;g_list_touch.max=list->InnerClipRect.Max;
                g_list_touch.visible=g_list_touch.min.x<g_list_touch.max.x&&g_list_touch.min.y<g_list_touch.max.y;
                g_list_touch.threshold=std::max(6.f,key_height*.22f);
                if(g_list_touch.delta!=0){ImGui::SetScrollY(std::clamp(ImGui::GetScrollY()+g_list_touch.delta,0.f,ImGui::GetScrollMaxY()));g_list_touch.delta=0;}
                g_list_touch.recording=true;
                for(const auto& item:s.items){
                    ImGui::PushID(item.id);
                    ImGui::BeginDisabled(!item.available);
                    ActionKey(item.name.c_str(),ImGui::GetContentRegionAvail().x,key_height,[id=item.id]{selected=id;InvalidateKeys();},item.id==selected);
                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
                g_list_touch.recording=false;
            }
            ImGui::EndChild();
        }
        ImGui::EndDisabled();
        if(std::any_of(s.items.begin(),s.items.end(),[](const auto& item){return !item.available;}))ImGui::TextWrapped("Shotgun Ammo kaynağı bulunamazsa oluşturulamaz.");
        ImGui::TextWrapped("Listeyi sürükle. Eşya ele gelir; elde aynısı varsa veya mermiyse önüne oluşur.");
        break;
    }
    case 2:{
        static int operation=0;
        static float clone_speed=1,clone_size=1,clone_distance=4;
        static size_t clone_kind=0;
        const char* operations[]={"Öldür","Kaldır","Klonla"};
        const float third=(content-ImGui::GetStyle().ItemSpacing.x*2)/3;
        for(int i=0;i<3;++i){if(i)ImGui::SameLine();ImGui::PushID(i+550);ActionKey(operations[i],third,key_height,[i]{operation=i;InvalidateKeys();},operation==i);ImGui::PopID();}
        ImGui::TextWrapped(operation==0?"Normal ölüm akışı; dönüşü oyun yönetir.":operation==1?"Düşmanı kaldırır; yeniden doğma çalışmaz.":"Ayrı ağ kimliğiyle yeni düşman oluşturur.");
        ImGui::BeginDisabled(!s.ready);
        if(operation==2){
            for(size_t k=0;k<3;++k){
                if(k)ImGui::SameLine();ImGui::PushID(static_cast<int>(k)+500);
                ActionKey(gameplay_bridge::kNames[k],third,key_height,[k]{clone_kind=k;InvalidateKeys();},clone_kind==k);ImGui::PopID();
            }
            ImGui::BeginDisabled(!s.enemies[clone_kind].clone_ready);
            ActionKey("Seçiliyi klonla",half,key_height,[queue,context=s.context,k=clone_kind,speed=clone_speed,size=clone_size,distance=clone_distance]{gameplay_bridge::Command c{gameplay_bridge::Kind::CloneEnemy,context};c.enemy=k;c.settings.speed=speed;c.settings.size=size;c.clone_distance=distance;queue(c);});
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!s.enemies[clone_kind].pause_ready||s.enemies[clone_kind].count==0);
            ActionKey(s.enemies[clone_kind].paused?"Devam et":"Durdur",half,key_height,[queue,context=s.context,k=clone_kind,on=!s.enemies[clone_kind].paused]{gameplay_bridge::Command c{gameplay_bridge::Kind::PauseEnemy,context};c.enemy=k;c.flag=on;queue(c);},s.enemies[clone_kind].paused);
            ImGui::EndDisabled();
            ImGui::Text("Kategorideki düşmanlar: %d",s.enemies[clone_kind].count);
            ImGui::TextWrapped("Durdur, seçilen türün asıllarını ve klonlarını etkiler.");
            if(!s.authority)ImGui::TextWrapped("Durdurma bu cihazda uygulanır.");
            ImGui::SliderFloat("Klon hızı",&clone_speed,.1f,5.f,"%.1fx");TrackSlider();
            ImGui::SliderFloat("Klon boyutu",&clone_size,.25f,3.f,"%.2fx");TrackSlider();
            ImGui::SliderFloat("Ön mesafe",&clone_distance,2.f,6.f,"%.1f m");TrackSlider();
        }else for(size_t k=0;k<gameplay_bridge::kEnemies;++k){
            if(k%2)ImGui::SameLine();ImGui::PushID(static_cast<int>(k)+500);
            ImGui::BeginDisabled(operation==0?!s.enemies[k].death_ready:s.enemies[k].count==0);
            ActionKey(gameplay_bridge::kNames[k],half,key_height,[queue,context=s.context,k,mode=operation]{gameplay_bridge::Command c{mode==0?gameplay_bridge::Kind::KillEnemy:gameplay_bridge::Kind::RemoveEnemy,context};c.enemy=k;queue(c);});
            ImGui::EndDisabled();ImGui::PopID();
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Kapalı seçenek kullanılamıyor. Ağdaki sonuç host yetkisine bağlıdır.");
        break;
    }
    case 3:
        ImGui::SeparatorText("Oyuncu dayanıklılığı");
        ImGui::BeginDisabled(!s.god_ready);
        ActionKey(s.god?"God Mode AÇIK - kapat":"God Mode KAPALI - aç",content,key_height,[queue,context=s.context,on=!s.god]{gameplay_bridge::Command c{gameplay_bridge::Kind::God,context};c.flag=on;queue(c);},s.god);
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s",s.god_status.c_str());
        ImGui::TextWrapped("Ölmeden önce aç. AI ölümünü engeller; diriltmez.");
        ImGui::TextWrapped("%s",s.invisible_status.empty()?"AI görünmezliği God Mode ile açılır.":s.invisible_status.c_str());
        ImGui::TextWrapped(s.fall_ready?"Düşme hasarı koruması God Mode ile açılır.":"Düşme koruması: oyuncu bağlantısı bekleniyor.");
        ImGui::TextWrapped(s.explosion_ready?"Benzin patlaması koruması God Mode'a bağlı.":"Benzin koruması hazır değil; tanı kaydını kontrol et.");
        ImGui::TextWrapped(s.infinity_ready?(s.infinity?"Infinity Ammo: açık (Crossbow / Shotgun).":"Infinity Ammo, God Mode ile açılır."):"Infinity Ammo: silah bağlantıları bekleniyor.");
        ImGui::TextWrapped(s.hamarat_ready?"Hamarat: hedefe bakıp etkileş. Desteklenen işlemlerde anahtar/alet gerekmez; mesafe ve ön koşullar geçerlidir.":"Hamarat bağlantıları bekleniyor.");
        break;
    case 8:{
        ImGui::BeginDisabled(!s.player_speed_ready);
        const auto change=[queue,context=s.context](float factor){gameplay_bridge::Command c{gameplay_bridge::Kind::PlayerSpeed,context};c.factor=factor;queue(c);};
        ActionKey("Normal hız (1x)",content,key_height,[change]{change(1);},s.player_speed==1);
        ActionKey("2x hız",half,key_height,[change]{change(2);},s.player_speed==2);ImGui::SameLine();
        ActionKey("3x hız",half,key_height,[change]{change(3);},s.player_speed==3);
        float factor=s.player_speed;ImGui::SetNextItemWidth(content);
        if(ImGui::SliderFloat("##player_speed",&factor,1.f,5.f,"Hız: %.1fx"))change(factor);TrackSlider();
        ImGui::EndDisabled();
        ImGui::TextWrapped(s.player_speed_ready?"Yürüme ve çömelme hızını ayarlar.":"Kendi maç karakterin bekleniyor.");
        break;
    }
    case 7:{
        static const char* names[]={"Easy","Normal","Hard","Extreme","Impossible","Practise"};
        ImGui::Text("Mevcut: %s",s.difficulty>=0&&s.difficulty<6?names[s.difficulty]:"Bilinmiyor");
        ImGui::BeginDisabled(!s.ready||!s.difficulty_ready);
        for(int i=0;i<6;++i){if(i%2)ImGui::SameLine();ActionKey(names[i],half,key_height,[queue,context=s.context,i]{gameplay_bridge::Command c{gameplay_bridge::Kind::Difficulty,context};c.item=i;queue(c);},s.difficulty==i);}
        ImGui::EndDisabled();
        ImGui::TextWrapped("Yalnız host. AI zorluğu yenilenir; harita, eşyalar ve ödül değişmez.");
        ImGui::TextWrapped("%s",s.status.c_str());break;
    }
    case 4:
        ImGui::BeginDisabled(!s.ghost_ready);
        ActionKey(s.ghost?"Hayalet modu AÇIK - kapat":"Hayalet modu KAPALI - aç",content,key_height,[queue,context=s.context,on=!s.ghost]{gameplay_bridge::Command c{gameplay_bridge::Kind::Ghost,context};c.flag=on;queue(c);},s.ghost);
        ImGui::EndDisabled();
        if(s.ghost){
            ActionKey("Başlangıca dön ve kapat",content,key_height,[queue,context=s.context]{queue({gameplay_bridge::Kind::GhostReturn,context});});
        }
        ImGui::TextWrapped("Joystick ile baktığın yöne uç. God Mode ayrıca açılır.");
        ImGui::TextWrapped("%s",s.tools_status.c_str());
        break;
    case 6:{
        static int selected=0,seconds=10;
        const auto& vote=s.vote;
        ImGui::TextWrapped("%s",vote.status.c_str());
        if(vote.active){
            ImGui::Text("EVET: %d | HAYIR: %d | %d sn",vote.yes,vote.no,vote.remaining);
            ActionKey("Oylamayı iptal et",content,key_height,[queue,context=s.context]{queue({gameplay_bridge::Kind::CancelVote,context});});
        }else{
            ImGui::BeginDisabled(!vote.ready);
            ImGui::SetNextItemWidth(content);ImGui::SliderInt("##vote_seconds",&seconds,1,60,u8"Oylama: %d saniye");TrackSlider();
            const auto player=std::find_if(vote.players.begin(),vote.players.end(),[&](const auto& p){return p.actor==selected&&p.actor!=vote.local;});
            ImGui::BeginDisabled(player==vote.players.end());
            ActionKey("Seçili oyuncu için oylama başlat",content,key_height,[queue,context=s.context,actor=selected,duration=seconds]{gameplay_bridge::Command c{gameplay_bridge::Kind::StartVote,context};c.item=actor;c.seconds=duration;queue(c);});
            ImGui::EndDisabled();
            if(ImGui::BeginChild("##vote_players",ImVec2(0,key_height*3),false)){
                for(const auto& p:vote.players){if(p.actor==vote.local)continue;ImGui::PushID(p.actor);
                    const auto label=p.name+" (#"+std::to_string(p.actor)+")";
                    ActionKey(label.c_str(),ImGui::GetContentRegionAvail().x,key_height,[actor=p.actor]{selected=actor;InvalidateKeys();},p.actor==selected);ImGui::PopID();
                }
            }ImGui::EndChild();ImGui::EndDisabled();
            ImGui::TextWrapped("/evet veya /hayır: kişi başına tek oy. Başlatan +1 evet; eşitlikte şaka yapılmaz.");
        }
        ImGui::TextWrapped("Bu bir sohbet şakasıdır; kimse gerçekten atılmaz.");
        break;
    }
    case 9:{
        static std::string note;
        const auto& vote=s.vote;
        ImGui::TextWrapped("Bir oyuncuya dokun: ismi panoya kopyalanır.");
        const auto room_name=chat_bot_bridge::Read().room_name;
        if(!room_name.empty()){
            ActionKey("Oda adını kopyala",content,key_height,[room_name]{const bool ok=g_copy&&g_copy(room_name);note=ok?"Oda kopyalandı: "+room_name:"Oda adı kopyalanamadı.";InvalidateKeys();});
            ImGui::TextWrapped("Oda: %s",room_name.c_str());
        }
        if(!note.empty())ImGui::TextWrapped("%s",note.c_str());
        if(vote.players.empty())ImGui::TextWrapped("Odadaki oyuncu listesi bekleniyor.");
        if(ImGui::BeginChild("##copy_players",ImVec2(0,key_height*4),false)){
            for(const auto& p:vote.players){
                ImGui::PushID(p.actor);
                const auto label=p.name+(p.actor==vote.local?" (sen)":"");
                ActionKey(label.c_str(),ImGui::GetContentRegionAvail().x,key_height,[name=p.name]{const bool ok=g_copy&&g_copy(name);note=ok?"Kopyalandı: "+name:"Kopyalanamadı.";InvalidateKeys();});
                ImGui::PopID();
            }
        }ImGui::EndChild();
        if(!vote.players.empty())ImGui::TextWrapped("Arkadaşlık kodu bu oyun sürümünden henüz okunamıyor.");
        break;
    }
    default:break;
    }
}
void GameplayBody(float key_height,float content){
    static int previous_page=-2;
    if(previous_page!=g_gameplay_page){ImGui::SetScrollY(0);previous_page=g_gameplay_page;}
    if(g_gameplay_page>=0){
        static const char* titles[]={"Eşya spawn","","Düşman öldür","God Mode","Hayalet modu","","Oyuncu atma","Zorluk","Karakter hızı"};
        if(g_gameplay_page<9)ImGui::Text("%s",titles[g_gameplay_page]);else if(g_gameplay_page==9)ImGui::Text("İsim kopyala");
        GameplaySectionBody(key_height,content);
        return;
    }
    ImGui::TextWrapped("Oyun özellikleri");
    ImGui::TextWrapped("Bir bölüm seç.");
    const char* labels[]={"Eşya spawn","Düşman öldür","God Mode","Hayalet modu","Oyuncu atma","Zorluk","Karakter hızı","İsim kopyala"};
    const int pages[]={0,2,3,4,6,7,8,9};
    const float half=(content-ImGui::GetStyle().ItemSpacing.x)/2;
    for(int i=0;i<8;++i){
        if(i%2)ImGui::SameLine();
        ImGui::PushID(i+300);
        ActionKey(labels[i],half,key_height,[page=pages[i]]{g_gameplay_page=page;InvalidateKeys();});
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
            if(list.empty())ImGui::TextWrapped("Liste için oyun içi gardırobu aç.");
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
            ImGui::TextWrapped("Herkese göstermek için aynı odaya çık-gir yapar.");
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
void DrawSharePrompt(const ImGuiIO& io,float scale,float key_height){
    const auto share=photo_transport_bridge::Read();
    if(share.share_request){
        const float boxw=std::min(io.DisplaySize.x*.90f,420.f*scale);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f),ImGuiCond_Always,ImVec2(.5f,.5f));
        ImGui::SetNextWindowSize(ImVec2(boxw,0),ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(.95f);
        if(ImGui::Begin("##gmm_photo_share_request",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove)){
            ImGui::TextWrapped("GMM Connect | Paylasim istegi");
            ImGui::TextWrapped("%s fotograf listenizi gormek istiyor.",share.share_request_from.c_str());
            ImGui::TextWrapped("Sadece Pictures/GMMShare klasorundeki JPG/PNG fotograflar paylasilir. Bu cihazdan tek dosya isteyebilir.");
            const float actionw=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)/2.f;
            ImGui::PushID("share_approve");ActionKey("Kabul et",actionw,key_height,[]{photo_transport_bridge::AllowBrowse();InvalidateKeys();});ImGui::PopID();ImGui::SameLine();
            ImGui::PushID("share_deny");ActionKey("Reddet",actionw,key_height,[]{photo_transport_bridge::DenyBrowse();InvalidateKeys();});ImGui::PopID();
            RecordWindow();
        }
        ImGui::End();
    }
}
void DrawHub(const ImGuiIO& io){
    const float scale=std::clamp(std::min(io.DisplaySize.x,io.DisplaySize.y)/500.0f,.7f,1.6f);
    const float key_height=43*scale,width=std::min(io.DisplaySize.x*.50f,324*scale);
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
    if(!g_hub_open){DrawSharePrompt(io,scale,key_height);return;}
    const float panel_y=std::min(pos.y+d+6*scale,io.DisplaySize.y*.5f);
    const float max_height=std::max(80.0f*scale,io.DisplaySize.y-panel_y-4*scale);
    const float panel_x=std::clamp(pos.x,0.0f,std::max(0.0f,io.DisplaySize.x-width));
    ImGui::SetNextWindowPos(ImVec2(panel_x,panel_y),ImGuiCond_Always,ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(width,0),ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0,0),ImVec2(width,max_height));
    ImGui::SetNextWindowBgAlpha(.74f);
    const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize;
    if(ImGui::Begin("##hub",nullptr,flags)){
        auto* hub=ImGui::GetCurrentWindow();
        g_hub_touch.visible=true;g_hub_touch.recording=true;
        g_hub_touch.min=hub->InnerClipRect.Min;g_hub_touch.max=hub->InnerClipRect.Max;
        g_hub_touch.threshold=std::max(7.f,key_height*.23f);
        UpdateTouchScroll(g_hub_touch);
        const float content=ImGui::GetContentRegionAvail().x;
        const float half=(content-ImGui::GetStyle().ItemSpacing.x)/2;
        if(g_hub_tab==0&&g_bot_page>=0){BotBody(io,key_height,content,max_height);}
        else if(g_hub_tab==2){
            // Subpage is focused: hide unrelated top-level tabs to keep the panel short.
            if(g_gameplay_page<0)ActionKey(u8"< Ana menü",content,key_height,[]{g_hub_tab=0;InvalidateKeys();});
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
        g_hub_touch.recording=false;
        RenderTouchThumb(g_hub_touch,hub);
        RecordWindow();
    }
    ImGui::End();
    DrawSharePrompt(io,scale,key_height);
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
void SetCopyProvider(CopyProvider provider) { g_copy=provider; }
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
    g_touching_hub=false;
    for(const auto& r:g_slider_rects)if(x>=r.first.x&&x<r.second.x&&y>=r.first.y&&y<r.second.y)return false; // let ImGui drag the slider

    auto within=[&](const ListTouch& l){return l.visible&&x>=l.min.x&&x<l.max.x&&y>=l.min.y&&y<l.max.y;};
    auto within_bar=[&](const ListTouch& l){return l.visible&&l.scroll_max>1&&x>=l.bar_min.x&&x<l.bar_max.x&&y>=l.bar_min.y&&y<l.bar_max.y;};
    g_touching_hub=!(within(g_list_touch)||within_bar(g_list_touch))&&(within(g_hub_touch)||within_bar(g_hub_touch));
    auto& list=g_touching_hub?g_hub_touch:g_list_touch;
    const TouchKey* top=nullptr;for(auto i=g_touch_keys.rbegin();i!=g_touch_keys.rend();++i)if(x>=i->min.x&&x<i->max.x&&y>=i->min.y&&y<i->max.y){top=&*i;break;}
    if(list.visible&&list.scroll_max>1&&x>=list.bar_min.x&&x<list.bar_max.x&&y>=list.bar_min.y&&y<list.bar_max.y){
        list.down=true;list.dragged=true;list.thumb_down=true;list.tap={};list.start_x=x;list.start_y=list.last_y=y;
        const float range=std::max(1.f,(list.track_bottom-list.track_top-list.thumb_height));
        list.scroll_set=std::clamp((y-list.track_top-list.thumb_height*.5f)/range,0.f,1.f)*list.scroll_max;
        return true;
    }
    if((!top||top->list)&&list.visible&&x>=list.min.x&&x<list.max.x&&y>=list.min.y&&y<list.max.y){
        list.down=true;list.dragged=false;list.start_x=x;list.start_y=list.last_y=y;list.tap={};
        for(const auto& key:g_touch_keys)if(key.list&&x>=key.min.x&&x<key.max.x&&y>=key.min.y&&y<key.max.y){list.tap=key.action;break;}
        return true;
    }
    if(top){
        const auto action=top->action;const bool repeat=top->repeat;
        g_repeat_min=top->min;g_repeat_max=top->max;g_touch_repeat=repeat?action:std::function<void()>{};
        g_repeat_at=ImGui::GetTime()+.22;g_refocus=true;action();return true;
    }
    return false;
}
void TouchHeld(float x,float y){
    auto& list=g_touching_hub?g_hub_touch:g_list_touch;
    if(list.down){
        if(list.thumb_down){
            const float range=std::max(1.f,list.track_bottom-list.track_top-list.thumb_height);
            list.scroll_set=std::clamp((y-list.track_top-list.thumb_height*.5f)/range,0.f,1.f)*list.scroll_max;
            list.last_y=y;return;
        }
        if(std::fabs(y-list.start_y)>=list.threshold||std::fabs(x-list.start_x)>=list.threshold){list.dragged=true;list.tap={};}
        if(list.dragged)list.delta+=list.last_y-y;
        list.last_y=y;return;
    }
    if(!g_touch_repeat||x<g_repeat_min.x||x>=g_repeat_max.x||y<g_repeat_min.y||y>=g_repeat_max.y)return;
    if(ImGui::GetTime()>=g_repeat_at){const auto action=g_touch_repeat;g_repeat_at=ImGui::GetTime()+.035;action();}
}
void TouchUp(){auto& list=g_touching_hub?g_hub_touch:g_list_touch;const auto tap=list.down&&!list.dragged?list.tap:std::function<void()>{};list.down=false;list.thumb_down=false;list.tap={};g_touch_repeat={};g_touching_hub=false;if(tap)tap();}
bool HitTest(float x,float y) {
    for(int i=0;i<g_rect_count;++i) if(x>=g_rect_min[i].x&&x<g_rect_max[i].x&&y>=g_rect_min[i].y&&y<g_rect_max[i].y) return true;
    return false;
}
void ResetTransientState() {
    g_bot_page=-1;
    InvalidateKeys();
    if(g_shortcut.Active()) { g_shortcut.Cancel(g_text,g_cursor,g_anchor); Dirty(); }
    g_reset_history=true; g_shift=g_caps=g_submit=g_refocus=false;
    g_quick_send=false;
    g_page=Page::Letters; g_last_shift=-1; g_limit_until=-1; g_rect_count=0;
    g_emoji_mask=0; g_emoji_ready=g_emoji_query_ok=g_emoji_request=false; g_emoji_page=0;
}
void Draw() {
    g_touch_keys.clear();g_slider_rects.clear();
    g_list_touch.visible=false;g_list_touch.recording=false;
    g_hub_touch.visible=false;g_hub_touch.recording=false;
    g_rect_count=0;
    const auto& io=ImGui::GetIO();
    if(io.DisplaySize.x<=0||io.DisplaySize.y<=0) return;
    if(g_visible) {if(g_shortcuts_only)DrawShortcutStrip(io);else DrawKeyboard(io);}
    else if(g_integrated){DrawHub(io);DrawAutoJoin(io);}
    else DrawShowButton(io);
    if(!g_list_touch.visible)g_list_touch={};
    if(!g_hub_touch.visible)g_hub_touch={};
}
}
