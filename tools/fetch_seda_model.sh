#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:?Usage: fetch_seda_model.sh <output-dir-for-gmm_seda>}"
BASE="https://huggingface.co/atasoglu/seda-v0.2-tiny/resolve/main"
mkdir -p "$OUT_DIR/c32"
fetch() {
  local remote="$1" local_path="$2"
  echo "Downloading ${remote}"
  curl --fail --location --retry 5 --retry-all-errors --connect-timeout 20 \
    "$BASE/$remote" -o "$local_path"
  test -s "$local_path"
}
fetch "tokens.txt" "$OUT_DIR/tokens.txt"
fetch "c32/encoder.int8.onnx" "$OUT_DIR/c32/encoder.int8.onnx"
fetch "c32/decoder.onnx" "$OUT_DIR/c32/decoder.onnx"
fetch "c32/joiner.int8.onnx" "$OUT_DIR/c32/joiner.int8.onnx"
cat > "$OUT_DIR/MODEL-INFO.txt" <<'INFO'
Model: atasoglu/seda-v0.2-tiny
Variant: c32 (0.64-second streaming chunks)
Language: Turkish (tr-TR), 16 kHz mono PCM
Runtime: sherpa-onnx C API / ONNX Runtime, Android arm64-v8a
Decoder: greedy_search, one CPU thread
License: CC BY-SA 4.0
Source: https://huggingface.co/atasoglu/seda-v0.2-tiny

The model is loaded locally. Audio is not uploaded to the ASR provider.
INFO
cat > "$OUT_DIR/MODEL-CREDITS.txt" <<'CREDITS'
SEDA v0.2-Tiny by Atasoglu
Source: https://huggingface.co/atasoglu/seda-v0.2-tiny
License: Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
License text: https://creativecommons.org/licenses/by-sa/4.0/legalcode
The model files in this package are redistributed without modification.
CREDITS
