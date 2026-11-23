#pragma once
#include <string>
#if defined(__ANDROID__)
#include <jni.h>
namespace seda_assets {
// JNI_OnLoad passes the VM; dlopen injection can still discover the VM lazily.
void SetVM(JavaVM* vm);
// On the voice worker: copy APK assets/gmm_seda into app-internal files/gmm_seda,
// with atomic per-file writes and a package-version marker. Empty if unavailable.
std::string EnsureInstalled();
}
#else
namespace seda_assets {
inline std::string EnsureInstalled() { return {}; }
}
#endif
