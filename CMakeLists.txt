cmake_minimum_required(VERSION 3.22.1)
project(multiplayermod LANGUAGES C CXX)
if(NOT ANDROID)
    message(FATAL_ERROR "Use the Android NDK toolchain. Native editor tests are in tests/run.sh.")
endif()
if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "Only ANDROID_ABI=arm64-v8a is supported.")
endif()
option(MM_AUTO_UNITY "Start the IL2CPP/TMP keyboard integration when loaded" ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Vendor sources make CI reproducible and allow offline builds.
set(IMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
add_library(imgui STATIC
    ${IMGUI_SOURCE_DIR}/imgui.cpp ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_SOURCE_DIR})
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_ES3)
target_compile_definitions(imgui PUBLIC IMGUI_USE_WCHAR32)
target_link_libraries(imgui PUBLIC android log EGL GLESv3)
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(NearBranch ON CACHE BOOL "" FORCE) # Required for safe four-byte property getter hooks.
set(Plugin.SymbolResolver OFF CACHE BOOL "" FORCE)
set(Plugin.ImportTableReplace OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/dobby EXCLUDE_FROM_ALL)
add_library(multiplayermod SHARED src/keyboard_api.cpp src/keyboard_ui.cpp
    src/il2cpp_runtime.cpp src/emoji_support.cpp src/game_text_rules.cpp src/observer_target.cpp src/cosmetic_catalog.cpp src/cosmetic_room.cpp src/lobby_emotes.cpp src/chat_bot.cpp src/voice_ai.cpp src/seda_assets.cpp src/ad_block.cpp src/auto_join.cpp src/chat_bot_http.cpp src/gameplay.cpp src/unity_keyboard.cpp)
target_include_directories(multiplayermod PRIVATE include src)
target_compile_definitions(multiplayermod PRIVATE MM_AUTO_UNITY=$<BOOL:${MM_AUTO_UNITY}>)
target_compile_options(multiplayermod PRIVATE -Wall -Wextra -Wno-unused-parameter -fexceptions)
target_link_libraries(multiplayermod PRIVATE imgui dobby_static dl)

# SEDA model weights are bundled in APK assets/gmm_seda and extracted on demand
# to Context.getFilesDir()/gmm_seda by native code. The ONNX runtime and C API
# stay in APK lib/arm64-v8a (they are not Android assets).
option(MM_ENABLE_SEDA "Enable local Turkish SEDA v0.2-Tiny speech recognition" ON)
set(SHERPA_ONNX_ROOT "" CACHE PATH "sherpa-onnx Android install prefix (include/ and lib/)")
if(MM_ENABLE_SEDA)
    if(NOT SHERPA_ONNX_ROOT)
        message(FATAL_ERROR "MM_ENABLE_SEDA=ON requires -DSHERPA_ONNX_ROOT=<sherpa-onnx Android install prefix>")
    endif()
    # IMPORTANT: Use direct filesystem checks and absolute library paths.
    # find_path()/find_library() are intentionally forbidden here because the
    # Android toolchain can re-root PATHS into its sysroot and miss a valid
    # host-side install prefix produced by build-android-arm64-v8a.sh.
    get_filename_component(SHERPA_ONNX_ROOT_REAL "${SHERPA_ONNX_ROOT}" REALPATH)
    set(SHERPA_ONNX_INCLUDE_DIR "${SHERPA_ONNX_ROOT_REAL}/include")
    set(SHERPA_ONNX_C_API_LIBRARY "${SHERPA_ONNX_ROOT_REAL}/lib/libsherpa-onnx-c-api.so")
    set(SHERPA_ONNX_RUNTIME_LIBRARY "${SHERPA_ONNX_ROOT_REAL}/lib/libonnxruntime.so")
    set(SHERPA_ONNX_C_API_HEADER "${SHERPA_ONNX_INCLUDE_DIR}/sherpa-onnx/c-api/c-api.h")

    message(STATUS "SEDA_CMAKE_DIRECT_PATH_CHECK_V2: using sherpa-onnx prefix: ${SHERPA_ONNX_ROOT_REAL}")
    foreach(required_file IN ITEMS
            "${SHERPA_ONNX_C_API_HEADER}"
            "${SHERPA_ONNX_C_API_LIBRARY}"
            "${SHERPA_ONNX_RUNTIME_LIBRARY}")
        if(NOT EXISTS "${required_file}")
            message(FATAL_ERROR "SEDA dependency is missing: ${required_file}. Check the sherpa-onnx install step and SHERPA_ONNX_ROOT=${SHERPA_ONNX_ROOT_REAL}")
        endif()
    endforeach()
    message(STATUS "SEDA sherpa header: ${SHERPA_ONNX_C_API_HEADER}")
    message(STATUS "SEDA C API library: ${SHERPA_ONNX_C_API_LIBRARY}")
    message(STATUS "SEDA ONNX Runtime: ${SHERPA_ONNX_RUNTIME_LIBRARY}")

    target_include_directories(multiplayermod PRIVATE "${SHERPA_ONNX_INCLUDE_DIR}")
    target_compile_definitions(multiplayermod PRIVATE MM_ENABLE_SEDA=1)
    target_link_libraries(multiplayermod PRIVATE "${SHERPA_ONNX_C_API_LIBRARY}" "${SHERPA_ONNX_RUNTIME_LIBRARY}")
endif()

target_link_options(multiplayermod PRIVATE -Wl,--no-undefined -Wl,--exclude-libs,ALL
    -Wl,-z,max-page-size=16384 -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map)
set_target_properties(imgui multiplayermod PROPERTIES
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
set_target_properties(multiplayermod PROPERTIES OUTPUT_NAME "multiplayermod")
