#!/usr/bin/env bash
set -euo pipefail

# Push full Hytale "latest" package tree in to app-internal storage.
#
# Default source:
#   /home/critical/.var/app/com.hypixel.HytaleLauncher/data/Hytale/install/release/package/game/latest
#
# Default destination (inside app sandbox):
#   files/game
#
# Usage:
#   ./scripts/push_hytale_latest.sh
#   ./scripts/push_hytale_latest.sh --source /path/to/latest --package com.critical.vexaemulator
#   ./scripts/push_hytale_latest.sh --no-clean --min-files 20

PKG="com.critical.vexaemulator"
SOURCE_LATEST="/home/critical/.var/app/com.hypixel.HytaleLauncher/data/Hytale/install/release/package/game/latest"
DEST_GAME_DIR="files/game"
TMP_ROOT="/data/local/tmp/vexa-hytale-latest"
CLEAN_DEST=1
MIN_FILES=20

usage() {
  cat <<'EOF'
Usage: push_hytale_latest.sh [options]

Options:
  --source <dir>      Source "latest" directory on host
  --package <name>    Android package name (default: com.critical.vexaemulator)
  --dest <path>       Destination under run-as root (default: files/game)
  --no-clean          Keep existing destination content (default: clean destination first)
  --min-files <n>     Minimum number of files expected after copy (default: 20)
  --help              Show this help
EOF
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "[push-hytale-latest] missing command: $1" >&2
    exit 1
  }
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --source)
      SOURCE_LATEST="${2:?missing value for --source}"
      shift 2
      ;;
    --package)
      PKG="${2:?missing value for --package}"
      shift 2
      ;;
    --dest)
      DEST_GAME_DIR="${2:?missing value for --dest}"
      shift 2
      ;;
    --no-clean)
      CLEAN_DEST=0
      shift
      ;;
    --min-files)
      MIN_FILES="${2:?missing value for --min-files}"
      shift 2
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "[push-hytale-latest] unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

need_cmd adb
need_cmd find
need_cmd wc

if [[ ! -d "${SOURCE_LATEST}" ]]; then
  echo "[push-hytale-latest] source directory missing: ${SOURCE_LATEST}" >&2
  exit 1
fi

if [[ ! -f "${SOURCE_LATEST}/Client/HytaleClient" ]]; then
  echo "[push-hytale-latest] expected file missing: ${SOURCE_LATEST}/Client/HytaleClient" >&2
  exit 1
fi

adb get-state >/dev/null 2>&1 || {
  echo "[push-hytale-latest] no adb device connected" >&2
  exit 1
}

adb shell run-as "${PKG}" /system/bin/true >/dev/null 2>&1 || {
  echo "[push-hytale-latest] run-as failed for ${PKG} (debuggable app required)" >&2
  exit 1
}

SRC_NAME="$(basename "${SOURCE_LATEST}")"

echo "[push-hytale-latest] package=${PKG}"
echo "[push-hytale-latest] source=${SOURCE_LATEST}"
echo "[push-hytale-latest] dest=${DEST_GAME_DIR}"

adb shell /system/bin/rm -rf "${TMP_ROOT}" >/dev/null 2>&1 || true
adb shell /system/bin/mkdir -p "${TMP_ROOT}"
adb push "${SOURCE_LATEST}" "${TMP_ROOT}/" >/dev/null

if [[ "${CLEAN_DEST}" -eq 1 ]]; then
  adb shell run-as "${PKG}" /system/bin/rm -rf "${DEST_GAME_DIR}"
fi
adb shell run-as "${PKG}" /system/bin/mkdir -p "${DEST_GAME_DIR}"

# Copy directory contents into destination.
adb shell run-as "${PKG}" /system/bin/cp -R "${TMP_ROOT}/${SRC_NAME}/." "${DEST_GAME_DIR}"

# Keep compatibility with existing runtime path that may expect files/game/HytaleClient.
if adb shell run-as "${PKG}" /system/bin/test -f "${DEST_GAME_DIR}/Client/HytaleClient"; then
  adb shell run-as "${PKG}" /system/bin/cp "${DEST_GAME_DIR}/Client/HytaleClient" "${DEST_GAME_DIR}/HytaleClient"
  adb shell run-as "${PKG}" /system/bin/chmod 0755 "${DEST_GAME_DIR}/HytaleClient"
fi

adb shell /system/bin/rm -rf "${TMP_ROOT}" >/dev/null 2>&1 || true

REMOTE_FILES="$(
  adb shell run-as "${PKG}" find "${DEST_GAME_DIR}" -type f 2>/dev/null | tr -d '\r' | sed '/^$/d'
)"

if [[ -z "${REMOTE_FILES}" ]]; then
  echo "[push-hytale-latest] verification failed: no files found under ${DEST_GAME_DIR}" >&2
  exit 1
fi

FILE_COUNT="$(printf '%s\n' "${REMOTE_FILES}" | wc -l | tr -d '[:space:]')"
if [[ "${FILE_COUNT}" -lt "${MIN_FILES}" ]]; then
  echo "[push-hytale-latest] verification failed: expected >= ${MIN_FILES} files, got ${FILE_COUNT}" >&2
  exit 1
fi

adb shell run-as "${PKG}" /system/bin/test -f "${DEST_GAME_DIR}/Client/HytaleClient" || {
  echo "[push-hytale-latest] verification failed: ${DEST_GAME_DIR}/Client/HytaleClient missing" >&2
  exit 1
}

echo "[push-hytale-latest] verification ok: files=${FILE_COUNT}"
echo "[push-hytale-latest] key file: ${DEST_GAME_DIR}/Client/HytaleClient"
