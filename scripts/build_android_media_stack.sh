#!/usr/bin/env bash
set -euo pipefail

# Build full Android media stack used by VEXA thunk bring-up:
#   1) SDL3
#   2) SDL3_image (depends on SDL3 install)
#   3) openal-soft
#
# Environment overrides:
#   NDK_TOOLCHAIN, ANDROID_ABI, ANDROID_API
#   THIRD_PARTY_ROOT, BUILD_ROOT, INSTALL_ROOT

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

THIRD_PARTY_ROOT="${THIRD_PARTY_ROOT:-${PROJECT_ROOT}/third_party}"
BUILD_ROOT="${BUILD_ROOT:-${THIRD_PARTY_ROOT}/build-android-arm64}"
INSTALL_ROOT="${INSTALL_ROOT:-${THIRD_PARTY_ROOT}/install-android-arm64}"

NDK_TOOLCHAIN="${NDK_TOOLCHAIN:-/home/critical/Android/Sdk/ndk/29.0.14206865/build/cmake/android.toolchain.cmake}"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
ANDROID_API="${ANDROID_API:-30}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

export THIRD_PARTY_ROOT BUILD_ROOT INSTALL_ROOT
export NDK_TOOLCHAIN ANDROID_ABI ANDROID_API BUILD_TYPE

echo "[build-android-media-stack] third_party=${THIRD_PARTY_ROOT}"
echo "[build-android-media-stack] build_root=${BUILD_ROOT}"
echo "[build-android-media-stack] install_root=${INSTALL_ROOT}"
echo "[build-android-media-stack] abi=${ANDROID_ABI} api=${ANDROID_API}"

"${SCRIPT_DIR}/build_android_sdl3.sh"
"${SCRIPT_DIR}/build_android_sdl3_image.sh"
"${SCRIPT_DIR}/build_android_openal_soft.sh"

echo "[build-android-media-stack] done"
echo "[build-android-media-stack] outputs:"
find "${INSTALL_ROOT}" -type f | rg "libSDL3|libSDL3_image|libopenal" || true
