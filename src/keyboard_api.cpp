#include "keyboard_api.h"
#include "keyboard_bridge.h"
#include "keyboard_ui.h"
#include "text_buffer.h"
#include "chat_bot_http.h"
#include "keyboard_catalog.h"
#include "native_shortcuts.h"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace {
std::mutex g_mutex;
ImGuiContext* g_context = nullptr;
EGLContext g_egl = EGL_NO_CONTEXT;
ANativeWindow* g_window = nullptr;
int g_width = 0, g_height = 0;
bool g_down = false, g_captured = false, g_direct=false, g_native=false;
std::atomic<bool> g_render_ready{false};
std::chrono::steady_clock::time_point g_last;
struct CurrentContext {
    ImGuiContext* previous;
    CurrentContext() : previous(ImGui::GetCurrentContext()) { ImGui::SetCurrentContext(g_context); }
    ~CurrentContext() { ImGui::SetCurrentContext(previous); }
};
void Setup(float scale) {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 8; style.FrameRounding = 5;
    style.ItemSpacing = ImVec2(3, 3);
    style.WindowPadding = ImVec2(6, 6);
    style.ScaleAllSizes(scale);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.LogFilename = nullptr;
    io.KeyRepeatDelay = 0.22f; io.KeyRepeatRate = 0.035f;
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    static const ImWchar basic[]={0x20,0x17F,0};
    builder.AddRanges(basic); builder.AddChar(0x2588); builder.AddChar(0x2587); builder.AddChar(0x20AC);
    for (const auto& emoji:keyboard_catalog::kEmojis) builder.AddChar(static_cast<ImWchar>(emoji.codepoint));
    builder.BuildRanges(&ranges);
    const char* paths[] = {"/system/fonts/Roboto-Regular.ttf", "/system/fonts/RobotoStatic-Regular.ttf",
                           "/system/fonts/NotoSans-Regular.ttf", "/system/fonts/DroidSans.ttf"};
    const float pixels=std::round(15*scale);
    bool loaded=false;
    for (const char* path:paths) if (FILE* f=std::fopen(path,"rb")) {
        std::fclose(f);
        if (io.Fonts->AddFontFromFileTTF(path,pixels,nullptr,ranges.Data)) { loaded=true; break; }
    }
    if (!loaded) {
        ImFontConfig cfg; cfg.SizePixels=pixels; io.Fonts->AddFontDefault(&cfg);
        __android_log_print(ANDROID_LOG_WARN,"multiplayermod","System font unavailable; some preview glyphs may be missing");
    }
    const char* symbols[]={"/system/fonts/NotoSansSymbols2-Regular.ttf","/system/fonts/NotoSansSymbols-Regular.ttf"};
    for (const char* path:symbols) if (FILE* f=std::fopen(path,"rb")) {
        std::fclose(f);
        ImFontConfig merge; merge.MergeMode=true; merge.PixelSnapH=true;
        if (io.Fonts->AddFontFromFileTTF(path,pixels,&merge,ranges.Data)) break;
    }
}
void Destroy(bool current) {
    if (!g_context) return;
    ImGuiContext* old = g_context;
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(old);
    if (current) ImGui_ImplOpenGL3_Shutdown();
    else ImGui_ImplOpenGL3_ShutdownWithoutGL(); // old context has gone; never delete its GL ids in the new context
    ImGui::DestroyContext(old);
    g_context = nullptr; g_egl = EGL_NO_CONTEXT; g_window = nullptr;
    ImGui::SetCurrentContext(previous == old ? nullptr : previous);
    g_down = g_captured = g_direct = g_native = false; g_render_ready = false;
    keyboard_ui::ResetTransientState();
}
bool Init(int width, int height) {
    if (width <= 0 || height <= 0 || eglGetCurrentContext() == EGL_NO_CONTEXT) { g_render_ready = false; return false; }
    EGLint version = 0;
    if (!eglQueryContext(eglGetCurrentDisplay(), eglGetCurrentContext(), EGL_CONTEXT_CLIENT_VERSION, &version) || version < 3) { g_render_ready = false; return false; }
    if (g_context && g_egl != eglGetCurrentContext()) Destroy(false);
    if (!g_context) {
        ImGuiContext* previous = ImGui::GetCurrentContext();
        IMGUI_CHECKVERSION(); g_context = ImGui::CreateContext();
        if (!g_context) return false;
        Setup(std::clamp(std::min(width, height) / 500.0f, 0.7f, 1.6f));
        keyboard_ui::SetPasteProvider(&chat_bot_http::ReadClipboard);
        if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
            ImGui::DestroyContext(g_context); g_context = nullptr;
            ImGui::SetCurrentContext(previous); return false;
        }
        g_egl = eglGetCurrentContext();
        g_last = std::chrono::steady_clock::now();
        ImGui::SetCurrentContext(previous);
    }
    g_width = width; g_height = height;
    return true;
}
void Frame() {
    if (!g_context || g_egl != eglGetCurrentContext()) return;
    CurrentContext current;
    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(g_width), static_cast<float>(g_height));
    const auto now = std::chrono::steady_clock::now();
    io.DeltaTime = std::clamp(std::chrono::duration<float>(now - g_last).count(), 1.0f / 1000.0f, 0.1f);
    g_last = now;
    ImGui_ImplOpenGL3_NewFrame();
    if (!ImGui_ImplOpenGL3_IsDeviceReady()) { g_render_ready = false; return; }
    // The uploaded atlas remains in GL; discard redundant CPU pixel copies.
    // Font source/configuration stays available for a context-loss rebuild.
    io.Fonts->ClearTexData();
    ImGui::NewFrame(); keyboard_ui::Draw(); ImGui::Render();
    // Unity may leave an off-screen target bound after its final blit.
    GLint previousDraw = 0, previousRead = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDraw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, previousRead);
    g_render_ready = true;
}
bool PointerPixels(float x, float y, bool down) {
    if (!g_context) return false;
    CurrentContext current;
    if(down&&!g_down){
        g_captured=keyboard_ui::HitTest(x,y);
        g_direct=g_captured&&keyboard_ui::TouchDown(x,y);
    }
    if(g_direct){
        if(down&&g_down)keyboard_ui::TouchHeld(x,y);
        const bool consumed=g_captured;g_down=down;
        if(!down){keyboard_ui::TouchUp();g_captured=g_direct=false;}
        return consumed;
    }
    const bool consumed = g_captured;
    if (g_captured) {
        auto& io = ImGui::GetIO();
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        io.AddMousePosEvent(x, y);
        if (down != g_down) io.AddMouseButtonEvent(0, down);
    }
    g_down = down;
    if (!down) g_captured = false;
    return consumed;
}
}
namespace keyboard_bridge {
void SurfaceFrame(int width, int height) {
    std::lock_guard<std::mutex> lock(g_mutex);
    keyboard_ui::SetIntegrated(true);
    if (Init(width, height)) Frame();
}
bool RenderReady() { return g_render_ready.load(); }
void Open(const char* text, bool password) {
    std::lock_guard<std::mutex> lock(g_mutex);
    keyboard_ui::ResetTransientState();
    g_native=false;
    keyboard_ui::SetShortcutsOnly(true);
    keyboard_ui::SetIntegrated(true); keyboard_ui::SetPassword(password);
    keyboard_ui::SetText(text); keyboard_ui::SetVisible(true);
}
// Panel alanları (API anahtarı, kişilik) için: aynı klavye, TMP alanı olmadan.
// Bir TMP alanı odak alırsa Open() bayrağı düşürür ve PollNative false döner.
void OpenNative(const char* text, bool password) {
    std::lock_guard<std::mutex> lock(g_mutex);
    keyboard_ui::ResetTransientState();
    keyboard_ui::SetShortcutsOnly(false);
    keyboard_ui::SetIntegrated(true); keyboard_ui::SetPassword(password);
    keyboard_ui::SetText(text); keyboard_ui::SetVisible(true);
    g_native=true;
}
bool PollNative(char* text, size_t capacity, bool& submit, bool& visible) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!g_native)return false;
    if (capacity) { std::strncpy(text, keyboard_ui::GetCommittedText(), capacity - 1); text[capacity - 1] = 0; }
    submit = keyboard_ui::ConsumeSubmit() != 0; visible = keyboard_ui::IsVisible();
    return true;
}
void CloseNative() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!g_native)return;
    g_native=false;
    keyboard_ui::SetShortcutsOnly(false);
    keyboard_ui::SetVisible(false); keyboard_ui::SetText("");
    keyboard_ui::SetPassword(false); keyboard_ui::ConsumeSubmit();
    keyboard_ui::ConsumeQuickSend();
}
bool ConsumeEmojiRequest() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return keyboard_ui::ConsumeEmojiRequest();
}
void SetEmojiSupport(uint64_t mask, bool query_ok) {
    std::lock_guard<std::mutex> lock(g_mutex);
    keyboard_ui::SetEmojiSupport(mask, query_ok);
}
void ReconcileText(const char* expected, const char* actual) {
    std::lock_guard<std::mutex> lock(g_mutex);
    // A render frame can already contain newer input or a composer draft.
    // Never overwrite that newer edit while reconciling an older snapshot.
    keyboard_ui::ReconcileText(expected, actual);
}
void Close() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_native=false;
    keyboard_ui::SetShortcutsOnly(false);
    keyboard_ui::SetVisible(false); keyboard_ui::SetText("");
    keyboard_ui::SetPassword(false); keyboard_ui::ConsumeSubmit();
    keyboard_ui::ConsumeQuickSend();
}
void Snapshot(char* text, size_t capacity, bool& submit, bool& visible, bool& quick_send) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (capacity) { std::strncpy(text, keyboard_ui::GetCommittedText(), capacity - 1); text[capacity - 1] = 0; }
    submit = keyboard_ui::ConsumeSubmit() != 0; visible = keyboard_ui::IsVisible();
    quick_send=keyboard_ui::ConsumeQuickSend();
}
bool Pointer(float x, float y, bool down) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return PointerPixels(x * g_width, y * g_height, down);
}
}
extern "C" {
bool mm_init(ANativeWindow* window) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!window || !Init(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window))) return false;
    g_window = window; return true;
}
int32_t mm_handle_input(AInputEvent* event) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_context || !event || AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
    const int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    if (action != AMOTION_EVENT_ACTION_DOWN && action != AMOTION_EVENT_ACTION_MOVE &&
        action != AMOTION_EVENT_ACTION_UP && action != AMOTION_EVENT_ACTION_CANCEL) return g_captured;
    return PointerPixels(AMotionEvent_getX(event, 0), AMotionEvent_getY(event, 0),
                         action != AMOTION_EVENT_ACTION_UP && action != AMOTION_EVENT_ACTION_CANCEL);
}
void mm_frame() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_window) { g_width = ANativeWindow_getWidth(g_window); g_height = ANativeWindow_getHeight(g_window); }
    Frame();
}
void mm_shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Destroy(g_egl == eglGetCurrentContext());
}
void mm_set_visible(bool value) { std::lock_guard<std::mutex> lock(g_mutex); keyboard_ui::SetVisible(value); }
bool mm_is_visible() { std::lock_guard<std::mutex> lock(g_mutex); return keyboard_ui::IsVisible(); }
void mm_set_text(const char* text) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto state=native_shortcuts::Read();
    if(state.active&&!g_native)native_shortcuts::Queue(state.session,{native_shortcuts::Kind::ReplaceAll,text?text:""});
    else keyboard_ui::SetText(text);
}
const char* mm_get_text() {
    thread_local std::string snapshot;
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto state=native_shortcuts::Read();
    snapshot=state.active&&!g_native?state.text:keyboard_ui::GetText();return snapshot.c_str();
}
int32_t mm_consume_submit() { std::lock_guard<std::mutex> lock(g_mutex); return keyboard_ui::ConsumeSubmit(); }
}
