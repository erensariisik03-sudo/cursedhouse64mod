#pragma once
// Opt-in access to Android's system-managed "All files access" settings.
// This does not grant permissions, read private files or transfer anything.
#ifdef __ANDROID__
#include <jni.h>
#else
struct JavaVM;
#endif
namespace storage_access {
#ifdef __ANDROID__
void SetVM(JavaVM* vm);
bool Granted();
bool OpenSettings();
#else
inline void SetVM(JavaVM*) {}
inline bool Granted() { return false; }
inline bool OpenSettings() { return false; }
#endif
}
