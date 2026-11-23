#pragma once
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <unistd.h>

#if defined(MM_ENABLE_SEDA)
#include <sherpa-onnx/c-api/c-api.h>
#endif

// Local Turkish ASR adapter. It is initialized lazily on the voice worker,
// never from Photon/Unity audio callbacks. Model weights remain external files.
namespace seda_asr {
inline std::string Join(const std::string& a, const char* b) { return a + "/" + b; }
inline bool IsFile(const std::string& p) { return ::access(p.c_str(), R_OK) == 0; }
inline std::string PackageName() {
    std::ifstream in("/proc/self/cmdline", std::ios::binary);
    std::string p; std::getline(in, p, '\0');
    const auto colon = p.find(':'); if (colon != std::string::npos) p.resize(colon);
    if (p.empty()) return {};
    for (char c : p) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '.' || c == '_')) return {};
    return p;
}
inline std::string ModelDirectory() {
    const std::string package = PackageName();
    if (package.empty()) return {};
    const std::string candidates[] = {
        "/sdcard/Android/data/" + package + "/files/gmm_seda",
        "/storage/emulated/0/Android/data/" + package + "/files/gmm_seda",
        "/data/user/0/" + package + "/files/gmm_seda",
        "/data/data/" + package + "/files/gmm_seda"
    };
    for (const auto& dir : candidates) {
        if (IsFile(Join(dir, "tokens.txt")) && IsFile(Join(dir, "c32/encoder.int8.onnx")) &&
            IsFile(Join(dir, "c32/decoder.onnx")) && IsFile(Join(dir, "c32/joiner.int8.onnx"))) return dir;
    }
    return {};
}
#if defined(MM_ENABLE_SEDA)
inline std::mutex mutex;
inline const SherpaOnnxOnlineRecognizer* recognizer = nullptr;
inline std::string loaded_directory;
inline bool InitLocked() {
    if (recognizer) return true;
    const std::string dir = ModelDirectory();
    if (dir.empty()) return false;
    const std::string encoder = Join(dir, "c32/encoder.int8.onnx");
    const std::string decoder = Join(dir, "c32/decoder.onnx");
    const std::string joiner = Join(dir, "c32/joiner.int8.onnx");
    const std::string tokens = Join(dir, "tokens.txt");
    SherpaOnnxOnlineRecognizerConfig cfg{};
    cfg.feat_config.sample_rate = 16000;
    cfg.feat_config.feature_dim = 80;
    cfg.model_config.transducer.encoder = encoder.c_str();
    cfg.model_config.transducer.decoder = decoder.c_str();
    cfg.model_config.transducer.joiner = joiner.c_str();
    cfg.model_config.tokens = tokens.c_str();
    cfg.model_config.num_threads = 1;
    cfg.model_config.provider = "cpu";
    cfg.model_config.modeling_unit = "bpe";
    cfg.model_config.model_type = "zipformer2";
    // c32 is SEDA's 0.64 s streaming export. Greedy decoding avoids loading
    // the optional ~20 MB language-model bundle and keeps RAM/CPU usage low.
    cfg.decoding_method = "greedy_search";
    cfg.max_active_paths = 4;
    cfg.enable_endpoint = 0;
    recognizer = SherpaOnnxCreateOnlineRecognizer(&cfg);
    if (!recognizer) return false;
    loaded_directory = dir;
    return true;
}
inline std::string TranscribeLocked(const std::vector<float>& samples) {
    if (samples.empty() || samples.size() > 16000u * 12u || !InitLocked()) return {};
    const SherpaOnnxOnlineStream* stream = SherpaOnnxCreateOnlineStream(recognizer);
    if (!stream) return {};
    constexpr int32_t kChunk = 16000 / 5; // 200 ms feed chunks; model itself uses c32 windows.
    for (size_t at = 0; at < samples.size(); at += kChunk) {
        const auto n = static_cast<int32_t>(std::min<size_t>(kChunk, samples.size() - at));
        SherpaOnnxOnlineStreamAcceptWaveform(stream, 16000, samples.data() + at, n);
        while (SherpaOnnxIsOnlineStreamReady(recognizer, stream)) SherpaOnnxDecodeOnlineStream(recognizer, stream);
    }
    SherpaOnnxOnlineStreamInputFinished(stream);
    while (SherpaOnnxIsOnlineStreamReady(recognizer, stream)) SherpaOnnxDecodeOnlineStream(recognizer, stream);
    const SherpaOnnxOnlineRecognizerResult* result = SherpaOnnxGetOnlineStreamResult(recognizer, stream);
    std::string text = result && result->text ? result->text : "";
    if (result) SherpaOnnxDestroyOnlineRecognizerResult(result);
    SherpaOnnxDestroyOnlineStream(stream);
    return text;
}
#endif
inline std::string Transcribe(const std::vector<float>& samples) {
#if defined(MM_ENABLE_SEDA)
    std::lock_guard<std::mutex> lock(mutex);
    return TranscribeLocked(samples);
#else
    (void)samples;
    return {};
#endif
}
inline bool Available() {
#if defined(MM_ENABLE_SEDA)
    std::lock_guard<std::mutex> lock(mutex);
    return recognizer != nullptr || !ModelDirectory().empty();
#else
    return false;
#endif
}
} // namespace seda_asr
