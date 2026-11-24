#pragma once
// Opt-in access to Android's system-managed "All files access" settings.
// This does not grant permissions, read private files or transfer anything.
#include <cstddef>
#include <cstdint>
#include <string>
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
bool ShowToast(const char* message);
// Android 10+: save a received, user-approved image via MediaStore without
// needing broad storage access. Names are generated locally, not remote paths.
bool SaveImageToGallery(const uint8_t* data,size_t size,bool png,uint64_t id,std::string& path);
#else
inline void SetVM(JavaVM*) {}
inline bool Granted() { return false; }
inline bool OpenSettings() { return false; }
inline bool ShowToast(const char*) { return false; }
inline bool SaveImageToGallery(const uint8_t*,size_t,bool,uint64_t,std::string&) {return false;}
#endif
}
