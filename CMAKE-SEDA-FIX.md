# SEDA CMake integration fix v2

The prior Android CI log showed that sherpa-onnx itself compiled and installed successfully.
The header and shared libraries were present under the install prefix, but the mod CMake
configuration still ran `find_path()` / `find_library()` and reported them missing.

This v2 package makes the following changes:

1. `CMakeLists.txt` resolves `SHERPA_ONNX_ROOT` and checks the three required files
   directly with `EXISTS`; it does not call `find_path()` or `find_library()`.
2. The CMake configure log prints the unique marker
   `SEDA_CMAKE_DIRECT_PATH_CHECK_V2` and the resolved absolute paths.
3. GitHub Actions checks for that marker before spending time building sherpa-onnx.
   If the old checkout is still being built, CI stops immediately with an explicit error.
4. After sherpa installation, CI lists the installed API header and `.so` files for
   traceability before configuring the mod.

## How to verify the correct source is on GitHub

The Actions log must contain:

```text
SEDA CMake direct-path fix v2 is present.
SEDA_CMAKE_DIRECT_PATH_CHECK_V2: using sherpa-onnx prefix: ...
```

If the first marker is absent, the repository is still building an older `CMakeLists.txt`;
replace the project files from this ZIP and rerun the workflow.

This patch does not claim an Android build succeeded. The GitHub Actions run must still
complete the CMake configure, native link, and ELF alignment checks.
