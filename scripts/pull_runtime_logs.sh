#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./scripts/pull_runtime_logs.sh [package] [remote_subdir_under_files]
# Example:
#   ./scripts/pull_runtime_logs.sh com.critical.vexaemulator artifacts
#   ./scripts/pull_runtime_logs.sh com.critical.vexaemulator runtime_logs_dir

PKG="${1:-com.critical.vexaemulator}"
REMOTE_SUBDIR="${2:-artifacts}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TS="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${PROJECT_DIR}/logs/${TS}/${REMOTE_SUBDIR}"

mkdir -p "${OUT_DIR}"

echo "[pull-runtime-logs] package=${PKG}"
echo "[pull-runtime-logs] remote=files/${REMOTE_SUBDIR}"
echo "[pull-runtime-logs] local=${OUT_DIR}"

REMOTE_FILES="$({
  adb shell run-as "${PKG}" find "files/${REMOTE_SUBDIR}" -type f 2>/dev/null || true
} | tr -d '\r' | sed '/^$/d')"

if [[ -z "${REMOTE_FILES}" ]]; then
  echo "[pull-runtime-logs] no files found in files/${REMOTE_SUBDIR}"
  exit 0
fi

while IFS= read -r remote_file; do
  rel="${remote_file#files/${REMOTE_SUBDIR}/}"
  local_file="${OUT_DIR}/${rel}"
  mkdir -p "$(dirname "${local_file}")"

  echo "[pull-runtime-logs] pulling ${remote_file} -> ${local_file}"
  adb exec-out run-as "${PKG}" cat "${remote_file}" > "${local_file}" || {
    echo "[pull-runtime-logs] failed to pull ${remote_file}" >&2
  }
done <<< "${REMOTE_FILES}"

echo "[pull-runtime-logs] done"
