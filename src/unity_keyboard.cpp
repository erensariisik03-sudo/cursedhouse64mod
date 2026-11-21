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
#include "chat_send_gate.h"
#include "native_shortcuts.h"
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
uint64_t g_shortcuts_session=0;
bool g_normalizing=false;
Object* g_quick_target=nullptr;
using Unary = void (*)(Object*, const Method*);
using Event = void (*)(Object*, Object*, const Method*);
Unary g_activate = nullptr, g_update = nullptr, g_disable = nullptr;
Unary g_late = nullptr, g_changed = nullptr, g_submit_callback = nullptr;
Unary g_chat_send = nullptr;
Event g_deselect = nullptr;
using Swap = EGLBoolean (*)(EGLDisplay, EGLSurface);
Swap g_swap = nullptr;
struct Methods {
    const Method *activate{}, *update{}, *deselect{}, *disable{}, *late{}, *changed{};
    const Method *text{}, *setText{}, *hide{}, *setHide{}, *focused{}, *readOnly{}, *interactable{};
    const Method *inputType{}, *deactivate{}, *submit{}, *alive{}, *limit{}, *setLimit{};
    const Method *screenW{}, *screenH{}, *currentEventSystem{};
    const Method *touchCurrent{}, *primaryTouch{}, *touchPosition{}, *touchPress{}, *pressed{}, *readControl{};
    const Method *legacyPosition{}, *legacyPress{};
    const Method *messageQueue{}, *findChat{}, *arrayLength{}, *arrayItem{}, *active{}, *chatSend{};
    const Method *selectionAnchor{}, *selectionFocus{}, *setSelectionAnchor{}, *setSelectionFocus{};
    const Method *keyboard{}, *keyboardSelection{}, *setKeyboardSelection{}, *canGetSelection{}, *canSetSelection{}, *keyboardArea{};
    Class* chatClass{};
    Field *chatInput{}, *chatAllowed{};
} m;

Object* Target() { return g_target ? g_api.gchandle_get_target(g_target) : nullptr; }
struct ScopedRoot {
    Handle handle;
    explicit ScopedRoot(Object* object):handle(object?g_api.gchandle_new(object,false):0){}
    ~ScopedRoot(){if(handle)g_api.gchandle_free(handle);}
    Object* Get()const{return handle?g_api.gchandle_get_target(handle):nullptr;}
    ScopedRoot(const ScopedRoot&)=delete;
};
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
    native_shortcuts::End(g_shortcuts_session);g_shortcuts_session=0;
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
struct Selection { int cursor=0,anchor=0; };
struct NativeRange { int start,length; };
Selection ReadSelection(Object* object,const std::string& text) {
    const int end=native_shortcuts::ToUnits(text,static_cast<int>(text.size()));
    int cursor=g_api.Value<int>(m.selectionFocus,object,end);
    int anchor=g_api.Value<int>(m.selectionAnchor,object,cursor);
    ScopedRoot keyboard(g_api.Call(m.keyboard,object));
    if(keyboard.Get()&&g_api.Value<bool>(m.canGetSelection,keyboard.Get())) {
        bool ok=false;auto* range=g_api.Call(m.keyboardSelection,keyboard.Get(),nullptr,&ok);
        if(ok&&range){const auto r=*static_cast<NativeRange*>(g_api.object_unbox(range));
            if(r.start>=0&&r.length>=0&&r.start<=end&&r.length<=end-r.start){anchor=r.start;cursor=r.start+r.length;}}
    }
    return {native_shortcuts::ToBytes(text,std::clamp(cursor,0,end)),
            native_shortcuts::ToBytes(text,std::clamp(anchor,0,end))};
}
void SetSelection(Object* object,const std::string& text,Selection selection) {
    int cursor=native_shortcuts::ToUnits(text,selection.cursor);
    int anchor=native_shortcuts::ToUnits(text,selection.anchor);
    void* a[]={&anchor};void* c[]={&cursor};
    g_api.Call(m.setSelectionAnchor,object,a);g_api.Call(m.setSelectionFocus,object,c);
    ScopedRoot keyboard(g_api.Call(m.keyboard,object));
    if(keyboard.Get()&&g_api.Value<bool>(m.canSetSelection,keyboard.Get())) {
        NativeRange range{std::min(anchor,cursor),std::max(anchor,cursor)-std::min(anchor,cursor)};
        void* args[]={&range};g_api.Call(m.setKeyboardSelection,keyboard.Get(),args);
    }
}
float NativeBottom() {
    const int height=g_api.Value<int>(m.screenH);
    bool ok=false;auto* box=g_api.Call(m.keyboardArea,nullptr,nullptr,&ok);
    if(!ok||!box||height<=0)return 0;
    struct Rect { float x,y,width,height; };
    const auto r=*static_cast<Rect*>(g_api.object_unbox(box));
    // Some Android players return an empty keyboard area or resize the whole
    // render surface. Use a top-anchored strip when the reported area is unusable.
    if(r.height<=0||r.y<0||r.y+r.height>=height)return 0;
    return std::clamp(1.f-(r.y+r.height)/height,0.f,1.f);
}
bool Normalize(Object* object) {
    if(g_normalizing)return true;
    bool ok=false;const auto before=ReadText(object,&ok);
    if(!ok)return false;
    const auto clean=text_buffer::Clean(before.c_str());
    if(clean==before)return true;
    const uint64_t generation=g_generation;
    const auto selection=ReadSelection(object,before);
    struct Guard { Guard(){g_normalizing=true;}~Guard(){g_normalizing=false;} } guard;
    ScopedRoot text(reinterpret_cast<Object*>(g_api.string_new(clean.c_str())));
    if(!text.Get())return false;
    void* args[]={text.Get()};g_api.Call(m.setText,object,args,&ok);
    if(generation!=g_generation)return false;
    if(!ok)return false;
    const auto actual=ReadText(object,&ok);
    if(!ok||actual!=clean)return false;
    SetSelection(object,actual,selection);
    return generation==g_generation;
}
void Changed(Object* self,const Method* method) {
    // Native TMP polling can notify listeners before LateUpdate returns. Clamp
    // before forwarding those listeners, including the game's tag-filter path.
    if(Target()==self) {
        if(g_normalizing)return; // the correcting setter must not notify twice
        if(!Normalize(self)){if(Target()==self)End();return;}
    }
    g_changed(self,method);
}
void Submit(Object* self,const Method* method) {
    if(Target()==self&&!Normalize(self)){if(Target()==self)End();return;}
    g_submit_callback(self,method);
}
void PublishEditor(Object* object,bool limited=false) {
    bool ok=false;const auto text=ReadText(object,&ok);
    if(!ok){End();return;}
    const uint64_t generation=g_generation;
    const auto selection=ReadSelection(object,text);
    const float bottom=NativeBottom();
    if(generation!=g_generation)return;
    g_applied=text;
    native_shortcuts::Publish(g_shortcuts_session,text,selection.cursor,selection.anchor,bottom,limited);
}
void Activate(Object* self, const Method* method) {
    if (!g_ready.load(std::memory_order_acquire) ||
        g_api.Value<bool>(m.readOnly, self, true) || !g_api.Value<bool>(m.interactable, self, false)) {
        g_activate(self, method); return;
    }
    if (Target() == self) { SetHide(self, false); g_activate(self, method); return; }
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
    g_original_hide=original;g_original_limit=original_limit;g_limit_relaxed=true;g_target=handle;
    const uint64_t generation=++g_generation;
    // Let TMP open the real device keyboard. Both native and TMP UTF-16 caps
    // are relaxed before Open; callbacks enforce 1024 Unicode scalars instead.
    if(!SetHide(self,false)||!SetLimit(self,0)){End();g_activate(self,method);return;}
    g_activate(self,method);
    if(generation!=g_generation)return;
    if(!g_api.Value<bool>(m.focused,self,false)||!Normalize(self)){if(generation==g_generation)End();return;}
    g_shortcuts_session=native_shortcuts::Begin();
    PublishEditor(self);
    if(generation!=g_generation)return;
    keyboard_bridge::Open(g_applied.c_str(),g_api.Value<int>(m.inputType,self)==2);
    g_editing=true;
    LOGI("Android IME + shortcut strip opened; limit=1024 Unicode characters");
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
void SendChat(Object* self,const Method* method) {
    if(!self||!m.chatAllowed||!g_api.field_get_value||!g_api.field_set_value){g_chat_send(self,method);return;}
    ScopedRoot owner(self);
    if(!owner.Get()){g_chat_send(self,method);return;}
    bool before=false;g_api.field_get_value(owner.Get(),m.chatAllowed,&before);
    struct Permit {
        Object* owner;bool old,changed=false;
        Permit(Object* o,bool value):owner(o),old(value){
            if(!old){changed=true;bool yes=true;g_api.field_set_value(owner,m.chatAllowed,&yes);}
        }
        ~Permit(){if(changed)g_api.field_set_value(owner,m.chatAllowed,&old);}
    } permit(owner.Get(),before);
    g_chat_send(self,method);
}
bool QuickSend(Object* object,uint64_t generation) {
    if(g_api.Value<int>(m.inputType,object)==2)return false;
    ScopedRoot owner(ChatOwner(object));
    if(generation!=g_generation)return true;
    if(!owner.Get())return false;
    bool allowed=false;g_api.field_get_value(owner.Get(),m.chatAllowed,&allowed);
    if(!chat_send_gate::Available(allowed))return false;
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
    if(stillOwned!=object||!chat_send_gate::Available(allowed)||!Alive(owner.Get())){
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
        SetHide(object,false);
        if(generation==g_generation)g_activate(object,m.activate);
    }
    if(generation==g_generation&&!g_api.Value<bool>(m.focused,object))End();
    LOGI("v19 square shortcut: normal chat method %s; keyboard session retained if field alive",ok?"invoked":"failed");
    return true;
}
void PumpEditor() {
    Object* object=Target();if(!g_target)return;
    if(!Alive(object)||!g_api.Value<bool>(m.focused,object)){End();return;}
    const uint64_t generation=g_generation;
    if(g_api.Value<int>(m.limit,object,-1)!=0&&!SetLimit(object,0)){End();return;}
    if(generation!=g_generation)return;
    if(!Normalize(object)){if(generation==g_generation)End();return;}
    bool limited=false;
    const auto commands=native_shortcuts::Take(g_shortcuts_session);
    for(const auto& command:commands) {
        // Read after TMP has consumed the latest Android edit. Apply an operation
        // to the current selection, never a full renderer snapshot of old text.
        bool ok=false;const auto current=ReadText(object,&ok);
        if(generation!=g_generation)return;
        if(!ok){End();return;}
        auto selection=ReadSelection(object,current);
        if(generation!=g_generation)return;
        std::array<char,text_buffer::kCapacity> buffer{};
        std::memcpy(buffer.data(),current.c_str(),current.size()+1);
        if(command.kind==native_shortcuts::Kind::ReplaceAll){selection.anchor=0;selection.cursor=static_cast<int>(current.size());}
        const int lo=std::min(selection.cursor,selection.anchor),hi=std::max(selection.cursor,selection.anchor);
        const auto candidate=current.substr(0,lo)+command.text+current.substr(hi);
        if(text_buffer::Count(candidate.c_str())>text_buffer::kLimit){limited=true;continue;}
        if(candidate!=current) {
            text_buffer::Replace(buffer.data(),selection.cursor,selection.anchor,command.text.c_str());
            ScopedRoot text(reinterpret_cast<Object*>(g_api.string_new(buffer.data())));
            if(!text.Get())continue;
            void* args[]={text.Get()};g_api.Call(m.setText,object,args,&ok);
            if(generation!=g_generation)return;
            if(!ok){End();return;}
            const auto actual=ReadText(object,&ok);
            if(generation!=g_generation)return;
            if(!ok){End();return;}
            if(actual==buffer.data())SetSelection(object,actual,selection);
            if(generation!=g_generation)return;
        }
        if(command.quick_send)QuickSend(object,generation);
        if(generation!=g_generation)return;
    }
    PublishEditor(object,limited);
}
void LateUpdate(Object* self,const Method* method) {
    g_late(self,method);
    // The original LateUpdate polls native text, suggestions, selection and IME
    // composition. Commands run afterwards so fast Android typing wins first.
    if(g_ready.load(std::memory_order_acquire)&&Target()==self)PumpEditor();
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
        // Losing touch/render readiness must not close the native keyboard.
    }

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
    m.late=fm("LateUpdate");m.changed=fm("SendOnValueChanged");
    m.activate = fm("ActivateInputFieldInternal"); m.deselect = fm("OnDeselect", 1); m.disable = fm("OnDisable");
    m.update = g_api.FindMethod(system, "Update", 0);
    m.currentEventSystem = g_api.FindMethod(system, "get_current", 0);
    m.text = fm("get_text"); m.setText = fm("set_text", 1);
    m.limit = fm("get_characterLimit"); m.setLimit = fm("set_characterLimit", 1);
    m.hide = fm("get_shouldHideSoftKeyboard"); m.setHide = fm("set_shouldHideSoftKeyboard", 1);
    m.focused = fm("get_isFocused"); m.readOnly = fm("get_readOnly"); m.interactable = fm("IsInteractable");
    m.inputType = fm("get_inputType"); m.deactivate = fm("DeactivateInputField", 1); m.submit = fm("SendOnSubmit");
    m.selectionAnchor=fm("get_selectionStringAnchorPosition");m.selectionFocus=fm("get_selectionStringFocusPosition");
    m.setSelectionAnchor=fm("set_selectionStringAnchorPosition",1);m.setSelectionFocus=fm("set_selectionStringFocusPosition",1);
    m.keyboard=fm("get_touchScreenKeyboard");
    auto* native=g_api.Find("UnityEngine","TouchScreenKeyboard");
    m.keyboardSelection=g_api.FindMethod(native,"get_selection",0);m.setKeyboardSelection=g_api.FindMethod(native,"set_selection",1);
    m.canGetSelection=g_api.FindMethod(native,"get_canGetSelection",0);m.canSetSelection=g_api.FindMethod(native,"get_canSetSelection",0);
    m.keyboardArea=g_api.FindMethod(native,"get_area",0);
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
    const Method* required[] = {m.late,m.changed,m.activate, m.deselect, m.disable, m.update, m.currentEventSystem,
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
        Hook(Entry(m.changed), reinterpret_cast<void*>(Changed), g_changed, "TMP.SendOnValueChanged") &&
        Hook(Entry(m.submit), reinterpret_cast<void*>(Submit), g_submit_callback, "TMP.SendOnSubmit") &&
        Hook(Entry(m.late), reinterpret_cast<void*>(LateUpdate), g_late, "TMP.LateUpdate") &&
        Hook(Entry(m.activate), reinterpret_cast<void*>(Activate), g_activate, "TMP.ActivateInputFieldInternal");
    if(ready&&m.chatSend&&m.chatAllowed&&g_api.field_set_value){
        const bool installed=Hook(Entry(m.chatSend),reinterpret_cast<void*>(SendChat),g_chat_send,"ChatManager.SendMessage");
        chat_send_gate::ready.store(installed,std::memory_order_release);
    }
    if (ready) g_rules = game_text_rules::Install(g_api);
    if (ready) auto_join::Install(g_api); // dolu odaya otomatik katılma (JoinRoom adını ve 'Game full' hatasını izler)
    if (ready) ad_block::Install(g_api); // banner/geçiş reklamı engeli (ödüllü reklamlara dokunmaz)
    g_ready.store(ready, std::memory_order_release);
    LOGI("v19 Integration %s; native keyboard active; shortcut strip uses GLES and Unity touch input", ready ? "ready" : "disabled");
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
