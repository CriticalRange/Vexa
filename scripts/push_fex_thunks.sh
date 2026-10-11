#!/usr/bin/env bash
set -euo pipefail

# Pushes the current FEX thunk payloads (SDL3, SDL3_image, SDL2, OpenAL, GL, EGL,
# GLESv2, Vulkan) straight into the app's files/thunks directory over adb, without
# rebuilding the APK. Mirrors guest-tests/graphics/stage_runtime_assets.py.
#
# Usage:
#   ./scripts/push_fex_thunks.sh
#
# Environment overrides:
#   PKG                Android package name
#   FEX_ROOT           FEX repo root (default: ../FEX next to this repo)
#   BUILD_DIR          Android FEX build dir
#   HOST_DIR           Host thunk build output dir
#   GUEST_DIR          Guest thunk build output dir
#   TMP_PREFIX         Temp prefix under /data/local/tmp

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
VEXA_ROOT="${VEXA_ROOT:-$(cd -- "${SCRIPT_DIR}/.." && pwd)}"
PKG="${PKG:-com.critical.vexaemulator}"
FEX_ROOT="${FEX_ROOT:-$(cd -- "${VEXA_ROOT}/../FEX" && pwd)}"
BUILD_DIR="${BUILD_DIR:-${FEX_ROOT}/build-android-arm64-ninja}"
HOST_DIR="${HOST_DIR:-${BUILD_DIR}/HostLibs_64}"
GUEST_DIR="${GUEST_DIR:-${BUILD_DIR}/Guest}"
TMP_PREFIX="${TMP_PREFIX:-/data/local/tmp/vexa-thunks}"
THIRD_PARTY_ANDROID_ARM64_ROOT="${THIRD_PARTY_ANDROID_ARM64_ROOT:-${VEXA_ROOT}/third_party/install-android-arm64}"
OPENAL_HOST_LIB="${OPENAL_HOST_LIB:-${VEXA_ROOT}/third_party/openal-soft-android/build-android-arm64/libopenal.so}"

HOST_DST="files/thunks/host"
GUEST_DST="files/thunks/guest"

HOST_LIBS=(
  "libSDL3-host.so"
  "libSDL3_image-host.so"
  "libopenal-host.so"
  "libGL-host.so"
  "libEGL-host.so"
  "libGLESv2-host.so"
  "libvulkan-host.so"
  "libSDL2-host.so"
  "libSDL3.so"
  "libSDL3.so.1"
  "libSDL3_image.so"
  "libopenal.so"
  "libSDL2.so"
)

GUEST_LIBS=(
  "libSDL3-guest.so"
  "libSDL3_image-guest.so"
  "libSDL2-guest.so"
  "libopenal-guest.so"
  "libGL-guest.so"
  "libEGL-guest.so"
  "libGLESv2-guest.so"
  "libvulkan-guest.so"
)

host_source_for() {
  case "$1" in
    # Android host-native (AArch64) runtimes the SDL3/OpenAL overlays load.
    libSDL3.so|libSDL3.so.1) echo "${THIRD_PARTY_ANDROID_ARM64_ROOT}/sdl3/lib/libSDL3.so" ;;
    libSDL3_image.so)        echo "${THIRD_PARTY_ANDROID_ARM64_ROOT}/sdl3_image/lib/libSDL3_image.so" ;;
    # Self-copy of the host thunk: fexldr_init dlopen()s libSDL2.so and resolves
    # its FEX_SDL2_* exports, so the staged name differs from the build output.
    libSDL2.so)              echo "${HOST_DIR}/libSDL2-host.so" ;;
    libopenal.so)            echo "${OPENAL_HOST_LIB}" ;;
    *)                       echo "${HOST_DIR}/$1" ;;
  esac
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "[push-fex-thunks] missing command: $1" >&2
    exit 1
  }
}

check_device() {
  adb get-state >/dev/null 2>&1 || {
    echo "[push-fex-thunks] no adb device connected" >&2
    exit 1
  }
  adb shell run-as "${PKG}" /system/bin/true >/dev/null 2>&1 || {
    echo "[push-fex-thunks] run-as failed for ${PKG} (debuggable build required)" >&2
    exit 1
  }
}

# ELF e_machine (little endian u16 at offset 18): 62 = x86_64, 183 = AArch64.
elf_machine() {
  od -An -tu2 -j18 -N2 "$1" | tr -d ' \r\n'
}

check_local_files() {
  local bad=0 f src machine
  for f in "${HOST_LIBS[@]}"; do
    src="$(host_source_for "$f")"
    if [[ ! -f "${src}" ]]; then
      echo "[push-fex-thunks] missing host lib: ${src}" >&2
      bad=1
    elif [[ "$(elf_machine "${src}")" != "183" ]]; then
      echo "[push-fex-thunks] ${f} source is not AArch64: ${src}" >&2
      bad=1
    fi
  done
  for f in "${GUEST_LIBS[@]}"; do
    src="${GUEST_DIR}/${f}"
    if [[ ! -f "${src}" ]]; then
      echo "[push-fex-thunks] missing guest lib: ${src}" >&2
      bad=1
    elif [[ "$(elf_machine "${src}")" != "62" ]]; then
      echo "[push-fex-thunks] ${f} source is not x86_64: ${src}" >&2
      bad=1
    fi
  done
  [[ "${bad}" -eq 0 ]] || exit 1
}

# Guards against pushing stale/minimal guest payloads (dynstr contains exported names).
check_key_symbols() {
  local sdl3="${GUEST_DIR}/libSDL3-guest.so" sym
  for sym in SDL_GetPlatform SDL_InitSubSystem SDL_QuitSubSystem SDL_GetError SDL_ClearError \
             SDL_GL_GetProcAddress SDL_GL_MakeCurrent SDL_PollEvent SDL_GetRevision; do
    grep -aqF "${sym}" "${sdl3}" || {
      echo "[push-fex-thunks] libSDL3-guest.so missing ${sym}" >&2
      exit 1
    }
  done

  local sdl2="${GUEST_DIR}/libSDL2-guest.so"
  for sym in SDL_Init SDL_GL_CreateContext SDL_PollEvent; do
    grep -aqF "${sym}" "${sdl2}" || {
      echo "[push-fex-thunks] libSDL2-guest.so missing ${sym}" >&2
      exit 1
    }
  done
}

push_one() {
  local src="$1" dst_dir="$2" base="$3"
  local tmp="${TMP_PREFIX}-${base}"

  echo "[push-fex-thunks] push ${base}"
  adb push "${src}" "${tmp}" >/dev/null
  adb shell run-as "${PKG}" /system/bin/mkdir -p "${dst_dir}"
  adb shell run-as "${PKG}" /system/bin/cp "${tmp}" "${dst_dir}/${base}"
  adb shell run-as "${PKG}" /system/bin/chmod 0755 "${dst_dir}/${base}"
  adb shell /system/bin/rm -f "${tmp}" >/dev/null 2>&1 || true
}

show_remote_summary() {
  echo "[push-fex-thunks] remote host libs:"
  adb shell run-as "${PKG}" /system/bin/ls -l "${HOST_DST}" || true
  echo "[push-fex-thunks] remote guest libs:"
  adb shell run-as "${PKG}" /system/bin/ls -l "${GUEST_DST}" || true
}

main() {
  need_cmd adb
  need_cmd od

  echo "[push-fex-thunks] BUILD_DIR=${BUILD_DIR}"
  echo "[push-fex-thunks] HOST_DIR=${HOST_DIR}"
  echo "[push-fex-thunks] GUEST_DIR=${GUEST_DIR}"

  check_device
  check_local_files
  check_key_symbols

  local f
  for f in "${HOST_LIBS[@]}"; do
    push_one "$(host_source_for "$f")" "${HOST_DST}" "${f}"
  done
  for f in "${GUEST_LIBS[@]}"; do
    push_one "${GUEST_DIR}/${f}" "${GUEST_DST}" "${f}"
  done

  show_remote_summary
  echo "[push-fex-thunks] done"
}

main "$@"
