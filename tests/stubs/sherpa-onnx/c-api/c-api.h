#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SherpaOnnxOnlineTransducerModelConfig { const char* encoder; const char* decoder; const char* joiner; } SherpaOnnxOnlineTransducerModelConfig;
typedef struct SherpaOnnxOnlineParaformerModelConfig { const char* encoder; const char* decoder; } SherpaOnnxOnlineParaformerModelConfig;
typedef struct SherpaOnnxOnlineZipformer2CtcModelConfig { const char* model; } SherpaOnnxOnlineZipformer2CtcModelConfig;
typedef struct SherpaOnnxOnlineNemoCtcModelConfig { const char* model; } SherpaOnnxOnlineNemoCtcModelConfig;
typedef struct SherpaOnnxOnlineToneCtcModelConfig { const char* model; } SherpaOnnxOnlineToneCtcModelConfig;
typedef struct SherpaOnnxOnlineModelConfig {
  SherpaOnnxOnlineTransducerModelConfig transducer;
  SherpaOnnxOnlineParaformerModelConfig paraformer;
  SherpaOnnxOnlineZipformer2CtcModelConfig zipformer2_ctc;
  const char* tokens; int32_t num_threads; const char* provider; int32_t debug;
  const char* model_type; const char* modeling_unit; const char* bpe_vocab;
  const char* tokens_buf; int32_t tokens_buf_size;
  SherpaOnnxOnlineNemoCtcModelConfig nemo_ctc; SherpaOnnxOnlineToneCtcModelConfig t_one_ctc;
} SherpaOnnxOnlineModelConfig;
typedef struct SherpaOnnxFeatureConfig { int32_t sample_rate; int32_t feature_dim; } SherpaOnnxFeatureConfig;
typedef struct SherpaOnnxOnlineRecognizerConfig {
  SherpaOnnxFeatureConfig feat_config; SherpaOnnxOnlineModelConfig model_config;
  const char* decoding_method; int32_t max_active_paths; int32_t enable_endpoint;
} SherpaOnnxOnlineRecognizerConfig;
typedef struct SherpaOnnxOnlineRecognizer SherpaOnnxOnlineRecognizer;
typedef struct SherpaOnnxOnlineStream SherpaOnnxOnlineStream;
typedef struct SherpaOnnxOnlineRecognizerResult { const char* text; } SherpaOnnxOnlineRecognizerResult;
static inline const SherpaOnnxOnlineRecognizer* SherpaOnnxCreateOnlineRecognizer(const SherpaOnnxOnlineRecognizerConfig*) { return 0; }
static inline const SherpaOnnxOnlineStream* SherpaOnnxCreateOnlineStream(const SherpaOnnxOnlineRecognizer*) { return 0; }
static inline void SherpaOnnxOnlineStreamAcceptWaveform(const SherpaOnnxOnlineStream*, int32_t, const float*, int32_t) {}
static inline int32_t SherpaOnnxIsOnlineStreamReady(const SherpaOnnxOnlineRecognizer*, const SherpaOnnxOnlineStream*) { return 0; }
static inline void SherpaOnnxDecodeOnlineStream(const SherpaOnnxOnlineRecognizer*, const SherpaOnnxOnlineStream*) {}
static inline void SherpaOnnxOnlineStreamInputFinished(const SherpaOnnxOnlineStream*) {}
static inline const SherpaOnnxOnlineRecognizerResult* SherpaOnnxGetOnlineStreamResult(const SherpaOnnxOnlineRecognizer*, const SherpaOnnxOnlineStream*) { return 0; }
static inline void SherpaOnnxDestroyOnlineRecognizerResult(const SherpaOnnxOnlineRecognizerResult*) {}
static inline void SherpaOnnxDestroyOnlineStream(const SherpaOnnxOnlineStream*) {}
#ifdef __cplusplus
}
#endif
