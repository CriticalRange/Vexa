#!/usr/bin/env bash
set -euo pipefail

# Build openal-soft shared library for Android.
#
# Environment overrides:
#   NDK_TOOLCHAIN, ANDROID_ABI, ANDROID_API
#   THIRD_PARTY_ROOT, BUILD_ROOT, INSTALL_ROOT

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

THIRD_PARTY_ROOT="${THIRD_PARTY_ROOT:-${PROJECT_ROOT}/third_party}"
SRC_DIR="${SRC_DIR:-${THIRD_PARTY_ROOT}/openal-soft}"
BUILD_ROOT="${BUILD_ROOT:-${THIRD_PARTY_ROOT}/build-android-arm64}"
INSTALL_ROOT="${INSTALL_ROOT:-${THIRD_PARTY_ROOT}/install-android-arm64}"

NDK_TOOLCHAIN="${NDK_TOOLCHAIN:-/home/critical/Android/Sdk/ndk/29.0.14206865/build/cmake/android.toolchain.cmake}"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
ANDROID_API="${ANDROID_API:-30}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

if [[ ! -f "${NDK_TOOLCHAIN}" ]]; then
  echo "[build-android-openal] missing NDK toolchain: ${NDK_TOOLCHAIN}" >&2
  exit 1
fi
if [[ ! -f "${SRC_DIR}/CMakeLists.txt" ]]; then
  echo "[build-android-openal] missing openal-soft source tree: ${SRC_DIR}" >&2
  exit 1
fi

BUILD_DIR="${BUILD_ROOT}/openal"
PREFIX_DIR="${INSTALL_ROOT}/openal"

mkdir -p "${BUILD_DIR}" "${PREFIX_DIR}"

echo "[build-android-openal] src=${SRC_DIR}"
echo "[build-android-openal] build=${BUILD_DIR}"
echo "[build-android-openal] prefix=${PREFIX_DIR}"

cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${NDK_TOOLCHAIN}" \
  -DANDROID_ABI="${ANDROID_ABI}" \
  -DANDROID_PLATFORM="android-${ANDROID_API}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${PREFIX_DIR}" \
  -DALSOFT_UTILS=OFF \
  -DALSOFT_EXAMPLES=OFF \
  -DALSOFT_TESTS=OFF \
  -DALSOFT_INSTALL_UTILS=OFF \
  -DALSOFT_INSTALL_EXAMPLES=OFF \
  -DALSOFT_BACKEND_ALSA=OFF \
  -DALSOFT_BACKEND_OSS=OFF \
  -DALSOFT_BACKEND_PULSEAUDIO=OFF \
  -DALSOFT_BACKEND_PIPEWIRE=OFF \
  -DALSOFT_BACKEND_JACK=OFF \
  -DALSOFT_BACKEND_SDL2=OFF \
  -DALSOFT_BACKEND_SDL3=OFF \
  -DALSOFT_BACKEND_OBOE=ON \
  -DALSOFT_BACKEND_OPENSL=ON

cmake --build "${BUILD_DIR}" -j
cmake --install "${BUILD_DIR}"

echo "[build-android-openal] done"
