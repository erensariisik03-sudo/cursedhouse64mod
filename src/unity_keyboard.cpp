#include "il2cpp_runtime.h"
#include "keyboard_api.h"
#include "keyboard_bridge.h"
#include "text_buffer.h"
#include "emoji_support.h"
#include "game_text_rules.h"
#include "observer_target.h"
#include "cosmetic_catalog.h"
#include "cosmetic_room.h"
#include "chat_bot.h"
#include "ad_block.h"
#include "auto_join.h"
#include "chat_bot_http.h"
#include "chat_shortcut.h"
#include "dobby.h"
#include <EGL/egl.h>
#include <android/log.h>
#include <jni.h>
#include <dlfcn.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "multiplayermod", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "multiplayermod", __VA_ARGS__)
namespace {
using namespace il2cpp;
Api g_api{};
std::atomic<bool> g_ready{false}, g_input_ok{false}, g_editing{false};
std::once_flag g_start;
using Invoke = Object* (*)(const Method*, void*, void**, Object**);
Invoke g_invoke = nullptr;
std::atomic<bool> g_invoke_ready{false};
std::atomic<bool> g_install_claimed{false};
// Session state is used only on Unity's main thread. Rendering never calls managed code.
Handle g_target = 0;
uint64_t g_generation = 0;
bool g_original_hide = false, g_limit_relaxed = false;
int g_original_limit = 0;
game_text_rules::Status g_rules{};
std::string g_applied;
Object* g_quick_target=nullptr;
using Unary = void (*)(Object*, const Method*);
using Event = void (*)(Object*, Object*, const Method*);
Unary g_activate = nullptr, g_update = nullptr, g_disable = nullptr;
Event g_deselect = nullptr;
using Swap = EGLBoolean (*)(EGLDisplay, EGLSurface);
Swap g_swap = nullptr;
struct Methods {
    const Method *activate{}, *update{}, *deselect{}, *disable{};
    const Method *text{}, *setText{}, *hide{}, *setHide{}, *focused{}, *readOnly{}, *interactable{};
    const Method *inputType{}, *deactivate{}, *submit{}, *alive{}, *limit{}, *setLimit{};
    const Method *screenW{}, *screenH{}, *currentEventSystem{};
    const Method *touchCurrent{}, *primaryTouch{}, *touchPosition{}, *touchPress{}, *pressed{}, *readControl{};
    const Method *legacyPosition{}, *legacyPress{};
    const Method *messageQueue{}, *findChat{}, *arrayLength{}, *arrayItem{}, *active{}, *chatSend{};
    Class* chatClass{};
    Field *chatInput{}, *chatAllowed{};
} m;

Object* Target() { return g_target ? g_api.gchandle_get_target(g_target) : nullptr; }
bool Alive(Object* object) {
    if (!object) return false;
    void* args[] = {object};
    auto* boxed = g_api.Call(m.alive, nullptr, args);
    return boxed && *static_cast<bool*>(g_api.object_unbox(boxed));
}
bool SetHide(Object* object, bool hidden) {
    void* args[] = {&hidden}; bool ok = false;
    g_api.Call(m.setHide, object, args, &ok); return ok;
}
bool SetLimit(Object* object, int limit) {
    void* args[] = {&limit}; bool ok = false;
    g_api.Call(m.setLimit, object, args, &ok); return ok;
}
void End() {
    // Clear identity before any invocation that could call user code recursively.
    Object* object = Target(); const Handle handle = g_target;
    const bool original = g_original_hide, relaxed = g_limit_relaxed;
    const int old_limit = g_original_limit; g_limit_relaxed = false;
    g_target = 0; ++g_generation; g_editing = false; g_applied.clear();
    keyboard_bridge::Close();
    if (Alive(object)) {
        if (relaxed) SetLimit(object, old_limit);
        SetHide(object, original);
    }
    if (handle) g_api.gchandle_free(handle);
}
std::string ReadText(Object* object, bool* ok = nullptr) {
    return g_api.Utf8(reinterpret_cast<String*>(g_api.Call(m.text, object, nullptr, ok)));
}
void Activate(Object* self, const Method* method) {
    if (!g_ready.load(std::memory_order_acquire) || !keyboard_bridge::RenderReady() || !g_input_ok.load() ||
        g_api.Value<bool>(m.readOnly, self, true) || !g_api.Value<bool>(m.interactable, self, false)) {
        g_activate(self, method); return;
    }
    if (Target() == self) { SetHide(self, true); g_activate(self, method); return; }
    if (g_target) End();
    bool ok = false;
    auto* boxed = g_api.Call(m.hide, self, nullptr, &ok);
    if (!ok || !boxed) { g_activate(self, method); return; }
    const bool original = *static_cast<bool*>(g_api.object_unbox(boxed));
    auto* boxed_limit = g_api.Call(m.limit, self, nullptr, &ok);
    if (!ok || !boxed_limit) { g_activate(self, method); return; }
    const int original_limit = *static_cast<int*>(g_api.object_unbox(boxed_limit));
    const Handle handle = g_api.gchandle_new(self, false);
    if (!handle) { g_activate(self, method); return; }
    if (!SetHide(self, true)) { g_api.gchandle_free(handle); g_activate(self, method); return; }
    g_original_hide = original; g_original_limit = original_limit; g_limit_relaxed = true; g_target = handle;
    const uint64_t generation = ++g_generation;
    // TMP counts UTF-16 units. The custom buffer is the sole 1024-scalar editor
    // limit, so disable only this active field's built-in limit until blur.
    if (!SetLimit(self, 0)) { End(); g_activate(self, method); return; }
    g_activate(self, method);
    if (generation != g_generation) return;
    if (!g_api.Value<bool>(m.focused, self, false)) { End(); return; }
    g_applied = ReadText(self);
    keyboard_bridge::Open(g_applied.c_str(), g_api.Value<int>(m.inputType, self) == 2);
    g_editing = true;
    LOGI("Custom editor opened; limit=1024 Unicode characters");
}
void Deselect(Object* self, Object* event, const Method* method) {
    g_deselect(self, event, method);
    if (g_ready.load(std::memory_order_acquire) && Target() == self && g_quick_target!=self) End();
}
void Disable(Object* self, const Method* method) {
    if (g_ready.load(std::memory_order_acquire) && Target() == self) End();
    g_disable(self, method);
}
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
bool ReadPointer(float& x, float& y, bool& down) {
    // Prefer Input System: legacy Input can throw when Active Input Handling is New only.
    if (m.touchCurrent && m.primaryTouch && m.touchPosition && m.touchPress && m.pressed && m.readControl) {
        auto* screen = g_api.Call(m.touchCurrent);
        auto* touch = screen ? g_api.Call(m.primaryTouch, screen) : nullptr;
        auto* press = touch ? g_api.Call(m.touchPress, touch) : nullptr;
        auto* position = touch ? g_api.Call(m.touchPosition, touch) : nullptr;
        if (press && position) {
            bool validPress = false, validPos = false;
            auto* pressed = g_api.Call(m.pressed, press, nullptr, &validPress);
            void* args[] = {position};
            auto* boxed = g_api.Call(m.readControl, nullptr, args, &validPos);
            if (validPress && validPos && pressed && boxed) {
                const auto value = *static_cast<Vec2*>(g_api.object_unbox(boxed));
                x = value.x; y = value.y; down = *static_cast<bool*>(g_api.object_unbox(pressed)); return true;
            }
        }
    }
    // Optional fallback for games that still expose legacy touch-to-mouse simulation.
    static bool legacyFailed = false;
    if (!legacyFailed && m.legacyPosition && m.legacyPress) {
        bool a = false, b = false; int button = 0; void* args[] = {&button};
        auto* pressed = g_api.Call(m.legacyPress, nullptr, args, &a);
        auto* position = g_api.Call(m.legacyPosition, nullptr, nullptr, &b);
        if (a && b && pressed && position) {
            auto value = *static_cast<Vec3*>(g_api.object_unbox(position));
            x = value.x; y = value.y; down = *static_cast<bool*>(g_api.object_unbox(pressed)); return true;
        }
        legacyFailed = true; LOGE("Legacy Input unavailable; waiting for Input System Touchscreen.current");
    }
    return false;
}
struct ScopedRoot {
    Handle handle;
    explicit ScopedRoot(Object* object):handle(object?g_api.gchandle_new(object,false):0){}
    ~ScopedRoot(){if(handle)g_api.gchandle_free(handle);}
    Object* Get()const{return handle?g_api.gchandle_get_target(handle):nullptr;}
    ScopedRoot(const ScopedRoot&)=delete;
};
// Resolve the owner by its actual inputField reference, never by UI text/RVA.
Object* ChatOwner(Object* input) {
    if(!m.chatClass||!m.chatInput||!m.chatAllowed||!m.chatSend||!m.findChat||!m.arrayLength||!m.arrayItem||!m.active||
       !g_api.class_get_type||!g_api.type_get_object||!g_api.field_get_value)return nullptr;
    const auto* type=g_api.class_get_type(m.chatClass);
    ScopedRoot managedType(type?g_api.type_get_object(type):nullptr);
    if(!managedType.Get())return nullptr;
    int excludeInactive=0,noSorting=0;
    void* args[]={managedType.Get(),&excludeInactive,&noSorting};bool ok=false;
    ScopedRoot array(g_api.Call(m.findChat,nullptr,args,&ok));
    if(!ok||!array.Get())return nullptr;
    const int count=g_api.Value<int>(m.arrayLength,array.Get(),-1);
    if(count<0||count>32)return nullptr;
    Object* chosen=nullptr;
    for(int i=0;i<count;++i){
        void* index[]={&i};ScopedRoot candidate(g_api.Call(m.arrayItem,array.Get(),index,&ok));
        if(!ok)return nullptr;
        if(!Alive(candidate.Get())||!g_api.Value<bool>(m.active,candidate.Get()))continue;
        Object* field=nullptr;g_api.field_get_value(candidate.Get(),m.chatInput,&field);
        if(field!=input)continue;
        if(chosen)return nullptr;
        chosen=candidate.Get();
    }
    return chosen;
}
bool QuickSend(Object* object,uint64_t generation) {
    if(g_api.Value<int>(m.inputType,object)==2)return false;
    ScopedRoot owner(ChatOwner(object));
    if(generation!=g_generation)return true;
    if(!owner.Get())return false;
    bool allowed=false;g_api.field_get_value(owner.Get(),m.chatAllowed,&allowed);
    if(!allowed)return false; // Keep the game's own send cooldown.
    bool ok=false;
    ScopedRoot text(reinterpret_cast<Object*>(g_api.string_new(chat_shortcut::kPayload)));
    if(!text.Get())return false;
    void* args[]={text.Get()};g_api.Call(m.setText,object,args,&ok);
    if(generation!=g_generation)return true;
    if(!ok){End();return true;}
    const auto accepted=ReadText(object,&ok);
    if(generation!=g_generation)return true;
    if(!ok||accepted!=chat_shortcut::kPayload){
        if(ok){g_applied=accepted;keyboard_bridge::ReconcileText(chat_shortcut::kPayload,accepted.c_str());}
        else End();
        return true;
    }
    Object* stillOwned=nullptr;
    g_api.field_get_value(owner.Get(),m.chatInput,&stillOwned);
    g_api.field_get_value(owner.Get(),m.chatAllowed,&allowed);
    if(stillOwned!=object||!allowed||!Alive(owner.Get())){
        g_applied=accepted;return true;
    }
    // SendMessage was inspected in this exact game's native binary: it uses
    // the regular chat RPC, clears inputField and starts the normal cooldown.
    struct Guard{Object* prior;explicit Guard(Object* v):prior(g_quick_target){g_quick_target=v;}~Guard(){g_quick_target=prior;}}guard(object);
    g_api.Call(m.chatSend,owner.Get(),nullptr,&ok);
    if(generation!=g_generation)return true; // OnDisable/new target must still win.
    bool read_ok=false;const auto actual=ReadText(object,&read_ok);
    if(generation!=g_generation)return true;
    if(!read_ok||!Alive(object)){End();return true;}
    g_applied=actual;
    keyboard_bridge::ReconcileText(chat_shortcut::kPayload,actual.c_str());
    if(!g_api.Value<bool>(m.focused,object)&&generation==g_generation){
        SetHide(object,true);
        if(generation==g_generation)g_activate(object,m.activate);
    }
    if(generation==g_generation&&!g_api.Value<bool>(m.focused,object))End();
    LOGI("v19 square shortcut: normal chat method %s; keyboard session retained if field alive",ok?"invoked":"failed");
    return true;
}
void PumpEditor() {
    Object* object = Target(); if (!g_target) return;
    if (!Alive(object) || !g_api.Value<bool>(m.focused, object)) { End(); return; }
    const uint64_t generation = g_generation;
    if (g_api.Value<int>(m.limit, object, -1) != 0 && !SetLimit(object, 0)) { End(); return; }
    if (generation != g_generation) return;
    if (keyboard_bridge::ConsumeEmojiRequest()) {
        bool query_ok = false;
        const uint64_t mask = emoji_support::Probe(g_api, object, query_ok);
        if (generation != g_generation) return;
        keyboard_bridge::SetEmojiSupport(mask, query_ok);
    }
    std::array<char, text_buffer::kCapacity> buffer{};
    bool submit = false, visible = true, quick_send=false;
    keyboard_bridge::Snapshot(buffer.data(), buffer.size(), submit, visible,quick_send);
    if(quick_send&&visible&&!submit&&QuickSend(object,generation))return;
    if(generation!=g_generation)return;
    // Direct assignment deliberately bypasses Append()/Validate() and the field's keyboard rules.
    if (g_applied != buffer.data()) {
        auto* text = g_api.string_new(buffer.data());
        void* args[] = {text}; bool ok = false;
        g_api.Call(m.setText, object, args, &ok);
        if (generation != g_generation) return; // onValueChanged may change scenes/focus
        if (!ok) { End(); return; }
        bool read_ok = false;
        const std::string actual = ReadText(object, &read_ok);
        if (generation != g_generation) return;
        if (!read_ok) { End(); return; }
        if (actual != buffer.data()) keyboard_bridge::ReconcileText(buffer.data(), actual.c_str());
        g_applied = actual;
    }
    if (submit || !visible) {
        if (submit) g_api.Call(m.submit, object);
        if (generation != g_generation) return;
        bool clearSelection = false; void* args[] = {&clearSelection};
        g_api.Call(m.deactivate, object, args);
        if (generation == g_generation) End();
    }
}
void Update(Object* self, const Method* method) {
    if (!g_ready.load(std::memory_order_acquire)) { g_update(self, method); return; }
    // Multiple EventSystems may exist; drive input only from the current one.
    if (g_api.Call(m.currentEventSystem) != self) { g_update(self, method); return; }
    float x = 0, y = 0; bool down = false;
    const int width = g_api.Value<int>(m.screenW), height = g_api.Value<int>(m.screenH);
    const bool valid = width > 0 && height > 0 && ReadPointer(x, y, down);
    g_input_ok = valid;
    bool captured = false;
    if (valid) captured = keyboard_bridge::Pointer(x / width, 1.0f - y / height, down);
    else {
        keyboard_bridge::Pointer(0, 0, false);
        if (g_target) End();
    }
    if (g_target && !keyboard_bridge::RenderReady()) End();
    PumpEditor();
    // Pause scene discovery/mutations while Photon suspends its message queue
    // for a scene load. Release UI roots but retain the native desired strings.
    const bool loading=m.messageQueue&&!g_api.Value<bool>(m.messageQueue,nullptr,true);
    if(loading)cosmetic_room::Pump(g_api,true);
    else if (!g_editing.load()) { cosmetic_room::Pump(g_api); cosmetic_catalog::Pump(g_api); }
    // Skip uGUI processing for the gesture that began on the keyboard, including its UP.
    // This does not globally filter arbitrary gameplay InputAction listeners.
    if (!captured) g_update(self, method);
    if(!g_editing.load()){chat_bot::Pump(g_api);auto_join::Pump(g_api);}
}
EGLBoolean SwapBuffers(EGLDisplay display, EGLSurface surface) {
    if (eglGetCurrentContext() != EGL_NO_CONTEXT && eglGetCurrentSurface(EGL_DRAW) == surface) {
        EGLint width = 0, height = 0, client = 0;
        if (eglQueryContext(display, eglGetCurrentContext(), EGL_CONTEXT_CLIENT_TYPE, &client) &&
            client == EGL_OPENGL_ES_API && eglQuerySurface(display, surface, EGL_WIDTH, &width) &&
            eglQuerySurface(display, surface, EGL_HEIGHT, &height) && width >= 64 && height >= 64)
            keyboard_bridge::SurfaceFrame(width, height);
    }
    return g_swap(display, surface);
}
bool Resolve() {
    auto* field = g_api.Find("TMPro", "TMP_InputField");
    auto* system = g_api.Find("UnityEngine.EventSystems", "EventSystem");
    if (!field || !system) return false;
    auto fm = [&](const char* name, int count = 0) { return g_api.FindMethod(field, name, count); };
    m.activate = fm("ActivateInputFieldInternal"); m.deselect = fm("OnDeselect", 1); m.disable = fm("OnDisable");
    m.update = g_api.FindMethod(system, "Update", 0);
    m.currentEventSystem = g_api.FindMethod(system, "get_current", 0);
    m.text = fm("get_text"); m.setText = fm("set_text", 1);
    m.limit = fm("get_characterLimit"); m.setLimit = fm("set_characterLimit", 1);
    m.hide = fm("get_shouldHideSoftKeyboard"); m.setHide = fm("set_shouldHideSoftKeyboard", 1);
    m.focused = fm("get_isFocused"); m.readOnly = fm("get_readOnly"); m.interactable = fm("IsInteractable");
    m.inputType = fm("get_inputType"); m.deactivate = fm("DeactivateInputField", 1); m.submit = fm("SendOnSubmit");
    auto* screen = g_api.Find("UnityEngine", "Screen");
    m.screenW = g_api.FindMethod(screen, "get_width", 0); m.screenH = g_api.FindMethod(screen, "get_height", 0);
    m.alive = g_api.FindMethod(g_api.Find("UnityEngine", "Object"), "op_Implicit", 1);
    auto method = [&](const char* ns, const char* type, const char* name, int args = 0) {
        return g_api.FindMethod(g_api.Find(ns, type), name, args);
    };
    m.touchCurrent = method("UnityEngine.InputSystem", "Touchscreen", "get_current");
    m.primaryTouch = method("UnityEngine.InputSystem", "Touchscreen", "get_primaryTouch");
    m.touchPosition = method("UnityEngine.InputSystem.Controls", "TouchControl", "get_position");
    m.touchPress = method("UnityEngine.InputSystem.Controls", "TouchControl", "get_press");
    m.pressed = method("UnityEngine.InputSystem.Controls", "ButtonControl", "get_isPressed");
    m.readControl = method("UnityEngine.InputSystem", "InputControlExtensions", "ReadValueAsObject", 1);
    m.legacyPosition = method("UnityEngine", "Input", "get_mousePosition");
    m.legacyPress = method("UnityEngine", "Input", "GetMouseButton", 1);
    m.messageQueue=method("Photon.Pun","PhotonNetwork","get_IsMessageQueueRunning");
    m.chatClass=g_api.Find("","ChatManager");
    m.chatSend=g_api.FindMethod(m.chatClass,"SendMessage",0);
    // Unique three-argument overload; avoid the generic bool overload sharing
    // FindObjectsOfType's name/arity, and avoid sorting the result.
    m.findChat=method("UnityEngine","Object","FindObjectsByType",3);
    m.arrayLength=method("System","Array","get_Length");
    m.arrayItem=method("System","Array","GetValueImpl",1);
    m.active=method("UnityEngine","Behaviour","get_isActiveAndEnabled");
    if(m.chatClass&&g_api.class_get_field_from_name){
        m.chatInput=g_api.class_get_field_from_name(m.chatClass,"inputField");
        m.chatAllowed=g_api.class_get_field_from_name(m.chatClass,"canSendMessage");
    }
    const Method* required[] = {m.activate, m.deselect, m.disable, m.update, m.currentEventSystem,
        m.text, m.setText, m.hide, m.setHide, m.focused, m.readOnly, m.interactable,
        m.inputType, m.deactivate, m.submit, m.limit, m.setLimit, m.screenW, m.screenH, m.alive};
    for (const auto* entry : required) if (!entry || !Entry(entry)) return false;
    return (m.touchCurrent && m.primaryTouch && m.touchPosition && m.touchPress && m.pressed && m.readControl) ||
           (m.legacyPosition && m.legacyPress);
}
template<class T> bool Hook(void* address, void* replacement, T& original, const char* name) {
    LOGI("v19 hook begin: %s", name);
    if (!address || DobbyHook(address, replacement, reinterpret_cast<void**>(&original)) != 0 || !original) {
        LOGE("v19 hook failed: %s; original keyboard kept enabled", name); return false;
    }
    LOGI("v19 hook complete: %s", name);
    return true;
}
void InstallFromUnityFrame() {
    // This runs after Unity dispatched a real EventSystem/TMP frame message, on its
    // already-attached scripting thread. A non-null domain alone is not readiness.
    LOGI("v19 stage 3: Unity frame observed; resolving metadata on Unity thread");
    if (!Resolve()) {
        LOGE("v19 required managed methods missing; original keyboard kept enabled"); return;
    }
    LOGI("v19 stage 4: metadata resolved; installing keyboard hooks");
    const bool ready = Hook(reinterpret_cast<void*>(eglSwapBuffers), reinterpret_cast<void*>(SwapBuffers), g_swap, "eglSwapBuffers") &&
        Hook(Entry(m.update), reinterpret_cast<void*>(Update), g_update, "EventSystem.Update") &&
        Hook(Entry(m.deselect), reinterpret_cast<void*>(Deselect), g_deselect, "TMP.OnDeselect") &&
        Hook(Entry(m.disable), reinterpret_cast<void*>(Disable), g_disable, "TMP.OnDisable") &&
        Hook(Entry(m.activate), reinterpret_cast<void*>(Activate), g_activate, "TMP.ActivateInputFieldInternal");
    if (ready) g_rules = game_text_rules::Install(g_api);
    if (ready) auto_join::Install(g_api); // dolu odaya otomatik katılma (JoinRoom adını ve 'Game full' hatasını izler)
    if (ready) ad_block::Install(g_api); // banner/geçiş reklamı engeli (ödüllü reklamlara dokunmaz)
    g_ready.store(ready, std::memory_order_release);
    LOGI("v19 Integration %s; custom editor requires active GLES renderer and working Unity input", ready ? "ready" : "disabled");
}
bool IsUnityFrameMessage(const Method* method) {
    const char* name = g_api.method_get_name(method);
    if (!name || (std::strcmp(name, "Start") != 0 && std::strcmp(name, "Update") != 0 && std::strcmp(name, "LateUpdate") != 0)) return false;
    Class* klass = g_api.method_get_class(method);
    if (!klass) return false;
    const char* ns = g_api.class_get_namespace(klass);
    const char* type = g_api.class_get_name(klass);
    if (!ns || !type) return false;
    return (std::strcmp(ns, "UnityEngine.EventSystems") == 0 && std::strcmp(type, "EventSystem") == 0 &&
                (std::strcmp(name, "Start") == 0 || std::strcmp(name, "Update") == 0)) ||
           (std::strcmp(ns, "TMPro") == 0 && std::strcmp(type, "TMP_InputField") == 0 && std::strcmp(name, "LateUpdate") == 0);
}
Object* ManagedInvoke(const Method* method, void* self, void** args, Object** exception) {
    Object* result = g_invoke(method, self, args, exception);
    if (!g_invoke_ready.load(std::memory_order_acquire) || g_install_claimed.load() ||
        !method || !self || (exception && *exception)) return result;
    // Do not initialize inside an arbitrary class initializer or while IL2CPP starts.
    if (!IsUnityFrameMessage(method)) return result;
    bool expected = false;
    if (g_install_claimed.compare_exchange_strong(expected, true)) InstallFromUnityFrame();
    return result;
}
void Bootstrap() {
    LOGI("v19 Loaded: %zu-bit; waiting for libil2cpp; no background VM attach", sizeof(void*) * 8);
    void* library = nullptr;
    for (int i = 0; i < 4800 && !library; ++i) {
        library = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
        if (!library) std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    if (!library) { LOGE("v19 libil2cpp not loaded within timeout; integration stopped"); return; }
    LOGI("v19 stage 1: libil2cpp loaded; resolving exported function addresses only");
    if (!g_api.Load(library)) { LOGE("v19 API exports unavailable; integration stopped"); return; }
    LOGI("v19 stage 2: installing managed frame observer");
    unsigned veneer_hops=0;
    void* invoke_target=observer_target::Resolve(g_api,veneer_hops);
    if (!invoke_target) { LOGE("v19 observer target unsafe; no hook installed; original keyboard kept enabled"); return; }
    LOGI("v19 observer target: export=%p implementation=%p veneer-hops=%u", reinterpret_cast<void*>(g_api.runtime_invoke), invoke_target, veneer_hops);
    if (!Hook(invoke_target, reinterpret_cast<void*>(ManagedInvoke), g_invoke, "il2cpp_runtime_invoke implementation")) return;
    // Our own calls use the trampoline, so metadata setup cannot recursively re-enter.
    g_api.runtime_invoke = g_invoke;
    g_invoke_ready.store(true, std::memory_order_release);
    LOGI("v19 waiting for Unity EventSystem.Start/Update or TMP.LateUpdate; original keyboard remains enabled");
    // No domain/assembly lookup, VM thread attachment, or managed call on this worker.
    // Hook trampolines live for process lifetime. The loaded .so must not be dlclosed.
}
void Start() {
#if MM_AUTO_UNITY
    std::call_once(g_start, [] { std::thread(Bootstrap).detach(); });
#endif
}
}
extern "C" MM_EXPORT uint32_t mm_get_status() {
    return (g_ready.load() ? 1u : 0u) | (keyboard_bridge::RenderReady() ? 2u : 0u) |
           (g_input_ok.load() ? 4u : 0u) | (g_editing.load() ? 8u : 0u) |
           (g_ready.load(std::memory_order_acquire) && g_rules.input_filter ? 16u : 0u) |
           (g_ready.load(std::memory_order_acquire) && g_rules.display_filter ? 32u : 0u) |
           (g_ready.load(std::memory_order_acquire) && g_rules.username_limit ? 64u : 0u);
}
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) { chat_bot_http::SetVM(vm); Start(); return JNI_VERSION_1_6; }
__attribute__((constructor)) static void OnLibraryLoaded() { Start(); }
