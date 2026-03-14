#!/usr/bin/env bash
set -euo pipefail

# Build SDL3_image shared library for Android.
# Expects SDL3 to be installed first (default: third_party/install-android-arm64/sdl3).
#
# Environment overrides:
#   NDK_TOOLCHAIN, ANDROID_ABI, ANDROID_API
#   THIRD_PARTY_ROOT, BUILD_ROOT, INSTALL_ROOT

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

THIRD_PARTY_ROOT="${THIRD_PARTY_ROOT:-${PROJECT_ROOT}/third_party}"
SRC_DIR="${SRC_DIR:-${THIRD_PARTY_ROOT}/SDL_image}"
BUILD_ROOT="${BUILD_ROOT:-${THIRD_PARTY_ROOT}/build-android-arm64}"
INSTALL_ROOT="${INSTALL_ROOT:-${THIRD_PARTY_ROOT}/install-android-arm64}"
SDL3_PREFIX="${SDL3_PREFIX:-${INSTALL_ROOT}/sdl3}"

NDK_TOOLCHAIN="${NDK_TOOLCHAIN:-/home/critical/Android/Sdk/ndk/29.0.14206865/build/cmake/android.toolchain.cmake}"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
ANDROID_API="${ANDROID_API:-30}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

if [[ ! -f "${NDK_TOOLCHAIN}" ]]; then
  echo "[build-android-sdl3-image] missing NDK toolchain: ${NDK_TOOLCHAIN}" >&2
  exit 1
fi
if [[ ! -f "${SRC_DIR}/CMakeLists.txt" ]]; then
  echo "[build-android-sdl3-image] missing SDL_image source tree: ${SRC_DIR}" >&2
  exit 1
fi
if [[ ! -d "${SDL3_PREFIX}" ]]; then
  echo "[build-android-sdl3-image] SDL3 prefix not found: ${SDL3_PREFIX}" >&2
  echo "[build-android-sdl3-image] build SDL3 first using scripts/build_android_sdl3.sh" >&2
  exit 1
fi

BUILD_DIR="${BUILD_ROOT}/sdl3_image"
PREFIX_DIR="${INSTALL_ROOT}/sdl3_image"
SDL3_CMAKE_DIR="${SDL3_PREFIX}/lib/cmake/SDL3"
CMAKE_PREFIX_PATH_VALUE="${SDL3_CMAKE_DIR}"
# Avoid vendored libpng/zlib path in clones where SDL_image/external/* isn't populated.
# This keeps PNG/JPG loading via stb and unblocks Android configure/build.
SDLIMAGE_PNG_LIBPNG="${SDLIMAGE_PNG_LIBPNG:-OFF}"

if [[ ! -f "${SDL3_CMAKE_DIR}/SDL3Config.cmake" ]]; then
  echo "[build-android-sdl3-image] missing SDL3Config.cmake at: ${SDL3_CMAKE_DIR}" >&2
  echo "[build-android-sdl3-image] build/install SDL3 first via scripts/build_android_sdl3.sh" >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}" "${PREFIX_DIR}"

echo "[build-android-sdl3-image] src=${SRC_DIR}"
echo "[build-android-sdl3-image] build=${BUILD_DIR}"
echo "[build-android-sdl3-image] prefix=${PREFIX_DIR}"
echo "[build-android-sdl3-image] sdl3_prefix=${SDL3_PREFIX}"

cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${NDK_TOOLCHAIN}" \
  -DANDROID_ABI="${ANDROID_ABI}" \
  -DANDROID_PLATFORM="android-${ANDROID_API}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_VALUE}" \
  -DSDL3_DIR="${SDL3_CMAKE_DIR}" \
  -DCMAKE_INSTALL_PREFIX="${PREFIX_DIR}" \
  -DBUILD_SHARED_LIBS=ON \
  -DSDLIMAGE_INSTALL=ON \
  -DSDLIMAGE_SAMPLES=OFF \
  -DSDLIMAGE_TESTS=OFF \
  -DSDLIMAGE_VENDORED=ON \
  -DSDLIMAGE_AVIF=OFF \
  -DSDLIMAGE_JXL=OFF \
  -DSDLIMAGE_TIF=OFF \
  -DSDLIMAGE_WEBP=OFF \
  -DSDLIMAGE_PNG_LIBPNG="${SDLIMAGE_PNG_LIBPNG}"

cmake --build "${BUILD_DIR}" -j
cmake --install "${BUILD_DIR}"

echo "[build-android-sdl3-image] done"
