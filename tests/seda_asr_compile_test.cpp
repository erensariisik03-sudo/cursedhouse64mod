#include "seda_asr.h"
#include <cassert>
#include <iostream>
int main() {
    // Compile the production MM_ENABLE_SEDA code path against the documented
    // sherpa-onnx v2.0.3 C API surface. Runtime inference is covered by Android CI/device testing.
    assert(seda_asr::Transcribe({}).empty());
    std::cout << "SEDA C API adapter compile path: PASS\n";
}
