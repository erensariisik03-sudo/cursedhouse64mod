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
set(Plugin.SymbolResolver OFF CACHE BOOL "" FORCE)
set(Plugin.ImportTableReplace OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/dobby EXCLUDE_FROM_ALL)
add_library(multiplayermod SHARED src/keyboard_api.cpp src/keyboard_ui.cpp
    src/il2cpp_runtime.cpp src/emoji_support.cpp src/game_text_rules.cpp src/observer_target.cpp src/cosmetic_catalog.cpp src/cosmetic_room.cpp src/lobby_emotes.cpp src/chat_bot.cpp src/ad_block.cpp src/auto_join.cpp src/chat_bot_http.cpp src/unity_keyboard.cpp)
target_include_directories(multiplayermod PRIVATE include src)
target_compile_definitions(multiplayermod PRIVATE MM_AUTO_UNITY=$<BOOL:${MM_AUTO_UNITY}>)
target_compile_options(multiplayermod PRIVATE -Wall -Wextra -Wno-unused-parameter -fexceptions)
target_link_libraries(multiplayermod PRIVATE imgui dobby_static dl)
target_link_options(multiplayermod PRIVATE -Wl,--no-undefined -Wl,--exclude-libs,ALL
    -Wl,-z,max-page-size=16384 -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map)
set_target_properties(imgui multiplayermod PROPERTIES
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
set_target_properties(multiplayermod PROPERTIES OUTPUT_NAME "multiplayermod")
