#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./scripts/pull_runtime_logs.sh [--live] [package] [remote_subdir_under_files]
# Example:
#   ./scripts/pull_runtime_logs.sh com.critical.vexaemulator artifacts
#   ./scripts/pull_runtime_logs.sh com.critical.vexaemulator runtime_logs_dir
#   ./scripts/pull_runtime_logs.sh --live com.critical.vexaemulator artifacts

LIVE_MODE=0
POSITIONAL=()
while (($#)); do
  case "$1" in
    --live)
      LIVE_MODE=1
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [--live] [package] [remote_subdir_under_files]"
      exit 0
      ;;
    --)
      shift
      while (($#)); do
        POSITIONAL+=("$1")
        shift
      done
      ;;
    -*)
      echo "[pull-runtime-logs] unknown option: $1" >&2
      echo "Usage: $0 [--live] [package] [remote_subdir_under_files]" >&2
      exit 2
      ;;
    *)
      POSITIONAL+=("$1")
      shift
      ;;
  esac
done

if ((${#POSITIONAL[@]} > 2)); then
  echo "[pull-runtime-logs] too many positional arguments" >&2
  echo "Usage: $0 [--live] [package] [remote_subdir_under_files]" >&2
  exit 2
fi

PKG="${POSITIONAL[0]:-com.critical.vexaemulator}"
REMOTE_SUBDIR="${POSITIONAL[1]:-artifacts}"
REMOTE_FIND_DIR="files/${REMOTE_SUBDIR}"
MAPS_WAIT_SECS="${MAPS_WAIT_SECS:-12}"
MAPS_INTERVAL_SECS="${MAPS_INTERVAL_SECS:-0.25}"
MAPS_MAX_SNAPSHOTS="${MAPS_MAX_SNAPSHOTS:-12}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TS="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${PROJECT_DIR}/logs/${TS}/${REMOTE_SUBDIR}"

mkdir -p "${OUT_DIR}"

echo "[pull-runtime-logs] package=${PKG}"
echo "[pull-runtime-logs] remote=${REMOTE_FIND_DIR}"
echo "[pull-runtime-logs] local=${OUT_DIR}"
echo "[pull-runtime-logs] live_mode=${LIVE_MODE}"

capture_runtime_worker_maps() {
  local deadline=$((SECONDS + MAPS_WAIT_SECS))
  local captures=0
  local saw_runtime_worker=0

  echo "[pull-runtime-logs] maps-watch wait=${MAPS_WAIT_SECS}s interval=${MAPS_INTERVAL_SECS}s max=${MAPS_MAX_SNAPSHOTS}"

  while ((SECONDS <= deadline)); do
    local runtime_pid
    runtime_pid="$(adb shell pidof -s "${PKG}:runtime_worker" 2>/dev/null | tr -d '\r' || true)"

    if [[ -n "${runtime_pid}" ]]; then
      saw_runtime_worker=1
      local stamp maps_out
      stamp="$(date +%Y%m%d-%H%M%S)-${captures}"
      maps_out="${OUT_DIR}/runtime_worker.${stamp}.maps"

      if adb exec-out run-as "${PKG}" cat "/proc/${runtime_pid}/maps" > "${maps_out}" 2>/dev/null; then
        cp -f "${maps_out}" "${OUT_DIR}/runtime_worker.maps"
        captures=$((captures + 1))
        echo "[pull-runtime-logs] captured runtime_worker maps #${captures} pid=${runtime_pid} -> ${maps_out}"
      fi

      if ((captures >= MAPS_MAX_SNAPSHOTS)); then
        break
      fi
      sleep "${MAPS_INTERVAL_SECS}"
      continue
    fi

    if ((saw_runtime_worker == 1)); then
      break
    fi
    sleep "${MAPS_INTERVAL_SECS}"
  done

  if ((captures == 0)); then
    echo "[pull-runtime-logs] runtime_worker not observed during maps watch window"
  else
    echo "[pull-runtime-logs] runtime_worker maps snapshots captured=${captures}"
  fi
}

if ((LIVE_MODE == 1)); then
  capture_runtime_worker_maps
else
  echo "[pull-runtime-logs] maps-watch disabled (pass --live to enable)"
fi

REMOTE_FILES="$({
    adb shell run-as "${PKG}" find "${REMOTE_FIND_DIR}" -type f 2>/dev/null || true
  } | tr -d '\r' | sed '/^$/d' | sed '\#/shaders/#d')"

if [[ -z "${REMOTE_FILES}" ]]; then
  echo "[pull-runtime-logs] no files found in files/${REMOTE_SUBDIR}"
  exit 0
fi

while IFS= read -r remote_file; do
  rel="${remote_file#${REMOTE_FIND_DIR}/}"
  if [[ "${rel}" == "${remote_file}" ]]; then
    rel="${remote_file#./${REMOTE_FIND_DIR}/}"
  fi
  if [[ "${rel}" == "${remote_file}" ]]; then
    rel="${remote_file#*files/${REMOTE_SUBDIR}/}"
  fi
  local_file="${OUT_DIR}/${rel}"
  mkdir -p "$(dirname "${local_file}")"

  echo "[pull-runtime-logs] pulling ${remote_file} -> ${local_file}"
  adb exec-out run-as "${PKG}" cat "${remote_file}" > "${local_file}" || {
    echo "[pull-runtime-logs] failed to pull ${remote_file}" >&2
  }
done <<< "${REMOTE_FILES}"

echo "[pull-runtime-logs] done"
