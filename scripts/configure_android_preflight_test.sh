#!/usr/bin/env bash
set -euo pipefail

PKG="com.critical.vexaemulator"
DEFAULT_HYTALE_EXEC="/home/critical/.var/app/com.hypixel.HytaleLauncher/data/Hytale/install/release/package/game/latest/Client/HytaleClient"

HOST_EXECUTABLE="${1:-$DEFAULT_HYTALE_EXEC}"
HOST_ROOTFS_DIR="${2:-}"
HOST_THUNK_HOST_DIR="${3:-}"
HOST_THUNK_GUEST_DIR="${4:-}"
HOST_ARTIFACT_DIR="${5:-}"

APP_BASE="/data/user/0/${PKG}/files"
APP_GAME_DIR="${APP_BASE}/game"
APP_ROOTFS_DIR="${APP_BASE}/rootfs"
APP_THUNK_HOST_DIR="${APP_BASE}/thunks/host"
APP_THUNK_GUEST_DIR="${APP_BASE}/thunks/guest"
APP_ARTIFACT_DIR="${APP_BASE}/artifacts"
APP_EXECUTABLE="${APP_GAME_DIR}/HytaleClient"

run_as() {
  adb shell run-as "${PKG}" "$@"
}

need_adb() {
  command -v adb >/dev/null || { echo "adb not found"; exit 1; }
  adb get-state >/dev/null 2>&1 || { echo "No adb device connected"; exit 1; }
  run_as /system/bin/true >/dev/null 2>&1 || {
    echo "run-as failed for ${PKG} (install debug build first)"
    exit 1
  }
}

ensure_internal_layout() {
  run_as /system/bin/mkdir -p "${APP_GAME_DIR}"
  run_as /system/bin/mkdir -p "${APP_ROOTFS_DIR}"
  run_as /system/bin/mkdir -p "${APP_THUNK_HOST_DIR}"
  run_as /system/bin/mkdir -p "${APP_THUNK_GUEST_DIR}"
  run_as /system/bin/mkdir -p "${APP_ARTIFACT_DIR}"

  if ! run_as /system/bin/test -f "${APP_EXECUTABLE}"; then
    run_as /system/bin/touch "${APP_EXECUTABLE}"
    run_as /system/bin/chmod 0755 "${APP_EXECUTABLE}"
  fi
}

copy_if_provided() {
  if [[ ! -f "${HOST_EXECUTABLE}" ]]; then
    echo "Missing HytaleClient: ${HOST_EXECUTABLE}"
    exit 1
  fi
  adb push "${HOST_EXECUTABLE}" /data/local/tmp/vexa_exec >/dev/null
  run_as /system/bin/cp /data/local/tmp/vexa_exec "${APP_EXECUTABLE}"
  run_as /system/bin/chmod 0755 "${APP_EXECUTABLE}"
  adb shell rm -f /data/local/tmp/vexa_exec

  if [[ -n "${HOST_ROOTFS_DIR}" && -d "${HOST_ROOTFS_DIR}" ]]; then
    adb push "${HOST_ROOTFS_DIR}" /data/local/tmp/vexa_rootfs >/dev/null
    run_as /system/bin/cp -R /data/local/tmp/vexa_rootfs/. "${APP_ROOTFS_DIR}/"
    adb shell rm -rf /data/local/tmp/vexa_rootfs
  fi

  if [[ -n "${HOST_THUNK_HOST_DIR}" && -d "${HOST_THUNK_HOST_DIR}" ]]; then
    adb push "${HOST_THUNK_HOST_DIR}" /data/local/tmp/vexa_thunk_host >/dev/null
    run_as /system/bin/cp -R /data/local/tmp/vexa_thunk_host/. "${APP_THUNK_HOST_DIR}/"
    adb shell rm -rf /data/local/tmp/vexa_thunk_host
  fi

  if [[ -n "${HOST_THUNK_GUEST_DIR}" && -d "${HOST_THUNK_GUEST_DIR}" ]]; then
    adb push "${HOST_THUNK_GUEST_DIR}" /data/local/tmp/vexa_thunk_guest >/dev/null
    run_as /system/bin/cp -R /data/local/tmp/vexa_thunk_guest/. "${APP_THUNK_GUEST_DIR}/"
    adb shell rm -rf /data/local/tmp/vexa_thunk_guest
  fi

  if [[ -n "${HOST_ARTIFACT_DIR}" && -d "${HOST_ARTIFACT_DIR}" ]]; then
    adb push "${HOST_ARTIFACT_DIR}" /data/local/tmp/vexa_artifacts >/dev/null
    run_as /system/bin/cp -R /data/local/tmp/vexa_artifacts/. "${APP_ARTIFACT_DIR}/"
    adb shell rm -rf /data/local/tmp/vexa_artifacts
  fi
}

show_result() {
  echo "[VEXA] Internal runtime layout:"
  run_as /system/bin/ls -ld \
    "${APP_GAME_DIR}" \
    "${APP_ROOTFS_DIR}" \
    "${APP_THUNK_HOST_DIR}" \
    "${APP_THUNK_GUEST_DIR}" \
    "${APP_ARTIFACT_DIR}"
  run_as /system/bin/ls -l "${APP_EXECUTABLE}"
}

need_adb
ensure_internal_layout
copy_if_provided
show_result

echo "[DONE] ${PKG} internal runtime files/folders are ensured."
