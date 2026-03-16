#!/usr/bin/env bash
set -euo pipefail

# Pushes FEX thunk artifacts to app-internal storage and verifies critical symbols.
#
# Usage:
#   ./scripts/push_fex_thunks.sh
#
# Environment overrides:
#   PKG                Android package name
#   FEX_ROOT           FEX repo root
#   BUILD_DIR          Android FEX build dir
#   HOST_DIR           Host thunk build output dir
#   GUEST_DIR          Guest thunk build output dir
#   TMP_PREFIX         Temp prefix under /data/local/tmp

PKG="${PKG:-com.critical.vexaemulator}"
FEX_ROOT="${FEX_ROOT:-/home/critical/FEX}"
BUILD_DIR="${BUILD_DIR:-${FEX_ROOT}/build-android-arm64-ninja}"
HOST_DIR="${HOST_DIR:-${BUILD_DIR}/HostLibs_64}"
GUEST_DIR="${GUEST_DIR:-${BUILD_DIR}/Guest}"
TMP_PREFIX="${TMP_PREFIX:-/data/local/tmp/vexa-thunks}"

HOST_DST="files/thunks/host"
GUEST_DST="files/thunks/guest"

HOST_LIBS=(
  "libSDL3-host.so"
  "libSDL3_image-host.so"
  "libopenal-host.so"
  "libSDL3.so"
  "libSDL3_image.so"
  "libopenal.so"
)

GUEST_LIBS=(
  "libSDL3-guest.so"
  "libSDL3_image-guest.so"
  "libopenal-guest.so"
)

host_source_for() {
  local name="$1"
  case "$name" in
    libSDL3-host.so|libSDL3_image-host.so|libopenal-host.so)
      echo "${HOST_DIR}/${name}"
      ;;
    # Overlay aliases consumed by thunk config should resolve to guest thunk binaries.
    libSDL3.so)
      echo "${GUEST_DIR}/libSDL3-guest.so"
      ;;
    libSDL3_image.so)
      echo "${GUEST_DIR}/libSDL3_image-guest.so"
      ;;
    libopenal.so)
      echo "${GUEST_DIR}/libopenal-guest.so"
      ;;
    *)
      echo "${HOST_DIR}/${name}"
      ;;
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

check_local_files() {
  local missing=0
  local f
  for f in "${HOST_LIBS[@]}"; do
    local src
    src="$(host_source_for "$f")"
    if [[ ! -f "${src}" ]]; then
      echo "[push-fex-thunks] missing host lib: ${src}" >&2
      missing=1
    fi
  done
  for f in "${GUEST_LIBS[@]}"; do
    if [[ ! -f "${GUEST_DIR}/${f}" ]]; then
      echo "[push-fex-thunks] missing guest lib: ${GUEST_DIR}/${f}" >&2
      missing=1
    fi
  done
  if [[ "${missing}" -ne 0 ]]; then
    exit 1
  fi
}

check_key_symbols() {
  local guest_sdl3="${GUEST_DIR}/libSDL3-guest.so"

  # These checks prevent stale 2-symbol guest SDL3 thunk pushes.
  nm -D "${guest_sdl3}" | rg -q " SDL_GetPlatform$" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_GetPlatform" >&2
    exit 1
  }
  nm -D "${guest_sdl3}" | rg -q " SDL_InitSubSystem$" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_InitSubSystem" >&2
    exit 1
  }
  nm -D "${guest_sdl3}" | rg -q " SDL_SetMainReady$" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_SetMainReady" >&2
    exit 1
  }
  nm -D "${guest_sdl3}" | rg -q " SDL_QuitSubSystem$" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_QuitSubSystem" >&2
    exit 1
  }
  nm -D "${guest_sdl3}" | rg -q " SDL_GetError$" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_GetError" >&2
    exit 1
  }
  nm -D "${guest_sdl3}" | rg -q " SDL_ClearError$" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_ClearError" >&2
    exit 1
  }
}

push_one() {
  local src="$1"
  local dst_dir="$2"
  local base
  base="$(basename "${src}")"
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
  adb shell run-as "${PKG}" /system/bin/ls -l "${HOST_DST}" | rg "libSDL3|libopenal" || true
  echo "[push-fex-thunks] remote guest libs:"
  adb shell run-as "${PKG}" /system/bin/ls -l "${GUEST_DST}" | rg "libSDL3|libopenal" || true
}

main() {
  need_cmd adb
  need_cmd nm
  need_cmd rg

  check_device
  check_local_files
  check_key_symbols

  local f
  for f in "${HOST_LIBS[@]}"; do
    push_one "$(host_source_for "$f")" "${HOST_DST}"
  done
  for f in "${GUEST_LIBS[@]}"; do
    push_one "${GUEST_DIR}/${f}" "${GUEST_DST}"
  done

  show_remote_summary
  echo "[push-fex-thunks] done"
}

main "$@"
