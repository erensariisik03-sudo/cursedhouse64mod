cmake_minimum_required(VERSION 3.18.1)

project(multiplayermod LANGUAGES C CXX ASM)

if (NOT ANDROID)
    message(FATAL_ERROR "multiplayermod is an Android NDK project. Configure it with android.toolchain.cmake.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (ANDROID_ABI STREQUAL "armeabi-v7a")
    add_library(multiplayermod SHARED
        src/main.cpp
        src/arm_hook.cpp
    )
    target_compile_definitions(multiplayermod PRIVATE CH_ARM32=1)
    target_compile_options(multiplayermod PRIVATE -marm)

elseif (ANDROID_ABI STREQUAL "arm64-v8a")
    add_library(multiplayermod SHARED
        src/main64.cpp
        src/arm64_hook.cpp
    )
    target_compile_definitions(multiplayermod PRIVATE CH_ARM64=1)

else()
    message(FATAL_ERROR "Supported ABIs: armeabi-v7a, arm64-v8a")
endif()

target_include_directories(multiplayermod PRIVATE include)

find_library(log-lib log REQUIRED)
find_library(dl-lib dl REQUIRED)

target_link_libraries(multiplayermod PRIVATE
    ${log-lib}
    ${dl-lib}
)

message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "Android platform: ${ANDROID_PLATFORM}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
