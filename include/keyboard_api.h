#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <android/input.h>
#include <android/native_window.h>
#define MM_EXPORT __attribute__((visibility("default")))
#ifdef __cplusplus
extern "C" {
#endif
// Optional explicit host bridge. Auto mode drives rendering through eglSwapBuffers.
// Init/frame/shutdown require the host's current GLES 3 context on the render thread.
MM_EXPORT bool mm_init(ANativeWindow* window);
MM_EXPORT int32_t mm_handle_input(AInputEvent* event);
MM_EXPORT void mm_frame(void);
MM_EXPORT void mm_shutdown(void);
MM_EXPORT void mm_set_visible(bool visible);
MM_EXPORT bool mm_is_visible(void);
// UTF-8 input is sanitized and capped at 1024 Unicode scalar values.
MM_EXPORT void mm_set_text(const char* utf8_text);
// Thread-local snapshot; valid until this thread next calls mm_get_text.
MM_EXPORT const char* mm_get_text(void);
MM_EXPORT int32_t mm_consume_submit(void);
// Auto integration status bits: 1=IL2CPP hooks ready, 2=GLES renderer ready,
// 4=Unity input works, 8=field being edited. No text is logged.
MM_EXPORT uint32_t mm_get_status(void);
#ifdef __cplusplus
}
#endif
