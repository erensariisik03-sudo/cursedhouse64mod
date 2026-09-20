#!/usr/bin/env bash
set -euo pipefail

: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to your Android NDK directory}"

ABI="${1:-armeabi-v7a}"

echo "Building ABI: $ABI"

cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM=android-21 \
  -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON

cmake --build build --parallel
echo "Built: build/libmultiplayermod.so"
