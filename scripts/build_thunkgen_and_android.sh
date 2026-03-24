#!/usr/bin/env bash
set -euo pipefail

FEX_ROOT="${FEX_ROOT:-/home/critical/FEX}"
HOST_BUILD_DIR="${HOST_BUILD_DIR:-$FEX_ROOT/build-host-thunkgen-ninja}"
ANDROID_BUILD_DIR="${ANDROID_BUILD_DIR:-$FEX_ROOT/build-android-arm64-ninja}"
LLVM_VER="${LLVM_VER:-19}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CLEAN=0

while [[ $# -gt 0 ]]; do
  arg="$1"
  case "$arg" in
    --clean) CLEAN=1 ;;
    --llvm-ver)
      shift
      if [[ $# -eq 0 ]]; then
        echo "--llvm-ver requires a value (example: 19)" >&2
        exit 1
      fi
      LLVM_VER="$1"
      ;;
    --llvm-ver=*)
      LLVM_VER="${arg#*=}"
      ;;
    -h|--help)
      echo "Usage: $0 [--clean] [--llvm-ver <major>]"
      echo "  --clean   Delete host/android FEX build folders before configure/build"
      echo "  --llvm-ver  Pin host thunkgen toolchain to a specific LLVM major (default: 19)"
      exit 0
      ;;
    *)
      echo "Unknown arg: $arg" >&2
      echo "Usage: $0 [--clean] [--llvm-ver <major>]" >&2
      exit 1
      ;;
  esac
  shift
done

safe_remove_dir() {
  local dir="$1"
  if [[ -d "$dir" ]]; then
    echo "[clean] removing: $dir"
    rm -rf "$dir"
  else
    echo "[clean] skip (not found): $dir"
  fi
}

if [[ "$CLEAN" -eq 1 ]]; then
  safe_remove_dir "$HOST_BUILD_DIR"
  safe_remove_dir "$ANDROID_BUILD_DIR"
fi

echo "[1/4] configure thunkgen host"
if [[ -n "$LLVM_VER" ]]; then
  echo "[toolchain] host thunkgen pinned to LLVM $LLVM_VER"
fi
LLVM_VER="$LLVM_VER" "${PROJECT_ROOT}/scripts/configure_fex_thunkgen.sh"

echo "[2/4] configure android"
"${PROJECT_ROOT}/scripts/configure_fex_android.sh"

if [[ ! -d "$HOST_BUILD_DIR" ]]; then
  echo "[error] host build dir missing after configure: $HOST_BUILD_DIR" >&2
  exit 1
fi
if [[ ! -d "$ANDROID_BUILD_DIR" ]]; then
  echo "[error] android build dir missing after configure: $ANDROID_BUILD_DIR" >&2
  exit 1
fi

echo "[3/4] build thunkgen host target"
cmake --build "$HOST_BUILD_DIR" --target thunkgen -j

echo "[4/4] build android"
cmake --build "$ANDROID_BUILD_DIR" -j

echo "[done] thunkgen + android build complete"
