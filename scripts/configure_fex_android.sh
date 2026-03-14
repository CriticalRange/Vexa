#!/usr/bin/env bash

FEX_ROOT="${FEX_ROOT:-/home/critical/FEX}"
BUILD_DIR="${BUILD_DIR:-$FEX_ROOT/build-android-arm64-ninja}"
THUNKGEN_EXECUTABLE="${THUNKGEN_EXECUTABLE:-/home/critical/FEX/build-host-thunkgen-ninja/Bin/thunkgen}"
NDK_TOOLCHAIN="${NDK_TOOLCHAIN:-/home/critical/Android/Sdk/ndk/29.0.14206865/build/cmake/android.toolchain.cmake}"
BUILD_THUNKS="${BUILD_THUNKS:-ON}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$FEX_ROOT" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK_TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTHUNKGEN_EXECUTABLE="$THUNKGEN_EXECUTABLE" \
  -DENABLE_DESKTOP_GL_THUNKS=OFF \
  -DVEXA_SDL3_PREFIX=/home/critical/vexa/third_party/install-android-arm64/sdl3 \
  -DVEXA_SDL3_IMAGE_PREFIX=/home/critical/vexa/third_party/install-android-arm64/sdl3_image \
  -DVEXA_OPENAL_PREFIX=/home/critical/vexa/third_party/install-android-arm64/openal \
  -DENABLE_VULKAN_THUNKS=OFF \
  -DENABLE_ASSERTIONS=ON \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-30 \
  -DANDROID_STL=c++_shared \
  -DBUILD_FEXCONFIG=OFF \
  -DBUILD_FEX_LINUX_TESTS=OFF \
  -DBUILD_TESTING=OFF \
  -DBUILD_STEAM_SUPPORT=OFF \
  -DBUILD_THUNKS="$BUILD_THUNKS" \
  -DENABLE_GDB_SYMBOLS=OFF \
  -DENABLE_LTO=OFF \
  -DENABLE_CCACHE=OFF \
  -DENABLE_WERROR=OFF \
  -DENABLE_JEMALLOC=OFF \
  -DENABLE_JEMALLOC_GLIBC_ALLOC=OFF
