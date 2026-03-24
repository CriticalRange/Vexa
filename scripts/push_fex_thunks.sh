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
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
VEXA_ROOT="${VEXA_ROOT:-$(cd -- "${SCRIPT_DIR}/.." && pwd)}"
THIRD_PARTY_ANDROID_ARM64_ROOT="${THIRD_PARTY_ANDROID_ARM64_ROOT:-${VEXA_ROOT}/third_party/install-android-arm64}"

HOST_DST="files/thunks/host"
GUEST_DST="files/thunks/guest"

HOST_LIBS=(
  "libSDL3-host.so"
  "libSDL3_image-host.so"
  "libopenal-host.so"
  "libGL-host.so"
  "libEGL-host.so"
  "libSDL3.so"
  "libSDL3.so.1"
  "libSDL3_image.so"
  "libopenal.so"
  "libopenal.so.1"
  "libGL.so.1"
  "libEGL.so.1"
)

GUEST_LIBS=(
  "libSDL3-guest.so"
  "libSDL3_image-guest.so"
  "libopenal-guest.so"
  "libGL-guest.so"
  "libEGL-guest.so"
)

host_source_for() {
  local name="$1"
  case "$name" in
    libSDL3-host.so|libSDL3_image-host.so|libopenal-host.so)
      echo "${HOST_DIR}/${name}"
      ;;
    # VEXA_FIXES: Runtime overlays for SDL/OpenAL must load Android host-native
    # dependencies (AArch64), not guest thunk payloads (x86_64).
    libSDL3.so|libSDL3.so.1)
      echo "${THIRD_PARTY_ANDROID_ARM64_ROOT}/sdl3/lib/libSDL3.so"
      ;;
    libSDL3_image.so)
      echo "${THIRD_PARTY_ANDROID_ARM64_ROOT}/sdl3_image/lib/libSDL3_image.so"
      ;;
    libopenal.so|libopenal.so.1)
      echo "${THIRD_PARTY_ANDROID_ARM64_ROOT}/openal/lib/libopenal.so"
      ;;
    # VEXA_FIXES: Host thunk aliases must remain AArch64 host payloads.
    # Pushing x86_64 guest payloads here causes host dlopen failures:
    # "EM_X86_64 instead of EM_AARCH64" during fexldr_init_libGL/libEGL.
    libGL.so.1)
      echo "${HOST_DIR}/libGL-host.so"
      ;;
    libEGL.so.1)
      echo "${HOST_DIR}/libEGL-host.so"
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

check_host_overlay_arch() {
  local missing=0
  local f src
  local aliases=("libSDL3.so" "libSDL3.so.1" "libSDL3_image.so" "libopenal.so" "libopenal.so.1" "libGL.so.1" "libEGL.so.1")
  for f in "${aliases[@]}"; do
    src="$(host_source_for "$f")"
    local machine_line=""
    local file_line=""
    local is_aarch64=1

    if command -v readelf >/dev/null 2>&1; then
      machine_line="$(readelf -h "${src}" 2>/dev/null | rg "Machine:" | head -n 1 || true)"
      if printf "%s\n" "${machine_line}" | rg -qi "AArch64|ARM64"; then
        is_aarch64=0
      fi
    fi

    if [[ "${is_aarch64}" -ne 0 ]] && command -v file >/dev/null 2>&1; then
      file_line="$(file "${src}" 2>/dev/null || true)"
      if printf "%s\n" "${file_line}" | rg -qi "ARM aarch64|AArch64|ARM64"; then
        is_aarch64=0
      fi
    fi

    if [[ "${is_aarch64}" -ne 0 ]]; then
      echo "[push-fex-thunks] ${f} source is not AArch64: ${src}" >&2
      if [[ -n "${machine_line}" ]]; then
        echo "[push-fex-thunks] readelf: ${machine_line}" >&2
      fi
      if [[ -n "${file_line}" ]]; then
        echo "[push-fex-thunks] file: ${file_line}" >&2
      fi
      missing=1
    fi
  done
  if [[ "${missing}" -ne 0 ]]; then
    exit 1
  fi
}

check_key_symbols() {
  local guest_sdl3="${GUEST_DIR}/libSDL3-guest.so"
  local guest_gl="${GUEST_DIR}/libGL-guest.so"
  local guest_sdl3_symbols
  local guest_gl_symbols
  guest_sdl3_symbols="$(nm -D "${guest_sdl3}" | awk '{print $NF}')"
  guest_gl_symbols="$(nm -D "${guest_gl}" | awk '{print $NF}')"

  require_symbol_any() {
    local dump="$1"
    local err="$2"
    shift 2
    local sym
    for sym in "$@"; do
      if printf "%s\n" "${dump}" | rg -Fqx "${sym}"; then
        return 0
      fi
    done
    echo "[push-fex-thunks] ${err}" >&2
    return 1
  }

  # These checks prevent pushing stale/minimal SDL3 guest thunk payloads.
  printf "%s\n" "${guest_sdl3_symbols}" | rg -Fqx "SDL_GetPlatform" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_GetPlatform" >&2
    exit 1
  }
  printf "%s\n" "${guest_sdl3_symbols}" | rg -Fqx "SDL_InitSubSystem" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_InitSubSystem" >&2
    exit 1
  }
  printf "%s\n" "${guest_sdl3_symbols}" | rg -Fqx "SDL_QuitSubSystem" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_QuitSubSystem" >&2
    exit 1
  }
  printf "%s\n" "${guest_sdl3_symbols}" | rg -Fqx "SDL_GetError" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_GetError" >&2
    exit 1
  }
  printf "%s\n" "${guest_sdl3_symbols}" | rg -Fqx "SDL_ClearError" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_ClearError" >&2
    exit 1
  }
  printf "%s\n" "${guest_sdl3_symbols}" | rg -Fqx "SDL_GL_GetProcAddress" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_GL_GetProcAddress" >&2
    exit 1
  }
  printf "%s\n" "${guest_sdl3_symbols}" | rg -Fqx "SDL_GL_MakeCurrent" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_GL_MakeCurrent" >&2
    exit 1
  }
  printf "%s\n" "${guest_sdl3_symbols}" | rg -Fqx "SDL_PollEvent" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_PollEvent" >&2
    exit 1
  }
  printf "%s\n" "${guest_sdl3_symbols}" | rg -Fqx "SDL_GetRevision" || {
    echo "[push-fex-thunks] libSDL3-guest.so missing SDL_GetRevision" >&2
    exit 1
  }

  # VEXA_FIXES: Guest SDL3 thunk should not carry hard undefined GL imports,
  # otherwise the guest loader can fail before thunk overlay routing is active.
  if nm -D "${guest_sdl3}" | rg -q "^[[:space:]]*U[[:space:]]+gl"; then
    echo "[push-fex-thunks] libSDL3-guest.so has unresolved GL symbols (expected: none)" >&2
    nm -D "${guest_sdl3}" | rg "^[[:space:]]*U[[:space:]]+gl" | head -n 20 >&2 || true
    exit 1
  fi

  # VEXA_FIXES: Ensure Android GL thunk payload includes required proc bridge symbols.
  require_symbol_any "${guest_gl_symbols}" \
    "libGL-guest.so missing glX proc bridge (glXGetProcAddress/glXGetProcAddressARB/fexfn_pack_glXGetProcAddress)" \
    "glXGetProcAddress" "glXGetProcAddressARB" "fexfn_pack_glXGetProcAddress" || exit 1
  require_symbol_any "${guest_gl_symbols}" \
    "libGL-guest.so missing clear-depth bridge (glClearDepthf*/glClearDepth*/fexfn_pack_*)" \
    "glClearDepthf" "glClearDepthfOES" "fexfn_pack_glClearDepthf" "fexfn_pack_glClearDepthfOES" \
    "glClearDepth" "fexfn_pack_glClearDepth" || exit 1
  require_symbol_any "${guest_gl_symbols}" \
    "libGL-guest.so missing glFlush bridge (glFlush/fexfn_pack_glFlush)" \
    "glFlush" "fexfn_pack_glFlush" || exit 1
  require_symbol_any "${guest_gl_symbols}" \
    "libGL-guest.so missing glGetSynciv bridge (glGetSynciv/fexfn_pack_glGetSynciv)" \
    "glGetSynciv" "fexfn_pack_glGetSynciv" || exit 1
  require_symbol_any "${guest_gl_symbols}" \
    "libGL-guest.so missing glReadPixels bridge (glReadPixels/fexfn_pack_glReadPixels)" \
    "glReadPixels" "fexfn_pack_glReadPixels" || exit 1
  require_symbol_any "${guest_gl_symbols}" \
    "libGL-guest.so missing glReadBuffer bridge (glReadBuffer/fexfn_pack_glReadBuffer)" \
    "glReadBuffer" "fexfn_pack_glReadBuffer" || exit 1
  require_symbol_any "${guest_gl_symbols}" \
    "libGL-guest.so missing glVertexAttrib2f bridge (glVertexAttrib2f/fexfn_pack_glVertexAttrib2f)" \
    "glVertexAttrib2f" "fexfn_pack_glVertexAttrib2f" || exit 1
  require_symbol_any "${guest_gl_symbols}" \
    "libGL-guest.so missing glVertexAttribIPointer bridge (glVertexAttribIPointer/fexfn_pack_glVertexAttribIPointer)" \
    "glVertexAttribIPointer" "fexfn_pack_glVertexAttribIPointer" || exit 1
  require_symbol_any "${guest_gl_symbols}" \
    "libGL-guest.so missing glClearDepth" \
    "glClearDepth" "fexfn_pack_glClearDepth" || exit 1
}

push_one() {
  local src="$1"
  local dst_dir="$2"
  local base="${3:-$(basename "${src}")}"
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
  adb shell run-as "${PKG}" /system/bin/ls -l "${HOST_DST}" | rg "libSDL3|libopenal|libGL|libEGL" || true
  echo "[push-fex-thunks] remote guest libs:"
  adb shell run-as "${PKG}" /system/bin/ls -l "${GUEST_DST}" | rg "libSDL3|libopenal|libGL|libEGL" || true
}

main() {
  need_cmd adb
  need_cmd nm
  need_cmd rg

  echo "[push-fex-thunks] BUILD_DIR=${BUILD_DIR}"
  echo "[push-fex-thunks] HOST_DIR=${HOST_DIR}"
  echo "[push-fex-thunks] GUEST_DIR=${GUEST_DIR}"

  check_device
  check_local_files
  check_host_overlay_arch
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
