#pragma once
#include <cstddef>
#include <cstdint>
namespace keyboard_bridge {
void SurfaceFrame(int width, int height);
bool RenderReady();
// Open the shortcut strip for the active native TMP/Android editing session.
void Open(const char* text, bool password);
void Close();
// Standalone panel text field (no TMP field): legacy ImGui editor fallback.
// PollNative returns false when not active.
void OpenNative(const char* text, bool password);
bool PollNative(char* text, size_t capacity, bool& submit, bool& visible);
void CloseNative();
void ReconcileText(const char* expected, const char* actual);
bool ConsumeEmojiRequest();
void SetEmojiSupport(uint64_t mask, bool query_ok);
void Snapshot(char* text, size_t capacity, bool& submit, bool& visible, bool& quick_send);
// Coordinates are normalized, top-left origin. Returns capture for the entire gesture.
bool Pointer(float x, float y, bool down);
}
