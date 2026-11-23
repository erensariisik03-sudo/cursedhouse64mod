#pragma once
#include <cstdint>
#include <string>

// Internal UI module. Not thread-safe: callers (keyboard_api.cpp) serialize access.
namespace keyboard_ui {
// Optional clipboard source for the Paste key (set by the host; null = key does nothing).
using PasteProvider = bool (*)(std::string& out);
void SetPasteProvider(PasteProvider provider);
// Optional clipboard sink for "copy name" buttons (set by the host; null = copy fails).
using CopyProvider = bool (*)(const std::string& text);
void SetCopyProvider(CopyProvider provider);
// Must be called between ImGui::NewFrame() and ImGui::Render().
void Draw();
// Direct rendered-key dispatch; caller holds the UI mutex/context.
bool TouchDown(float x,float y);
void TouchHeld(float x,float y);
void TouchUp();
void SetVisible(bool visible);
void SetIntegrated(bool value);
void SetPassword(bool value);
// TMP fields use Android typing plus a shortcut strip; standalone panel edits
// retain the existing full ImGui keyboard.
void SetShortcutsOnly(bool value);
bool IsVisible();
void SetText(const char* text);
bool ReconcileText(const char* expected, const char* actual);
const char* GetText();
// Pending size/color templates stay in the preview until completed.
const char* GetCommittedText();
int ConsumeSubmit();
bool ConsumeQuickSend();
bool ConsumeEmojiRequest();
void SetEmojiSupport(uint64_t mask, bool query_ok);
// True if (x, y) in display pixels lies inside any window drawn last frame.
bool HitTest(float x, float y);
// Clears shift/caps/submit and hit-test state (text is kept).
void ResetTransientState();
}
