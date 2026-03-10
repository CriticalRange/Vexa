#!/usr/bin/env bash
set -euo pipefail

# Extracts a downloaded FEX .sqsh rootfs into the project .fex-emu/RootFS folder.
#
# Defaults (resolved from this script location, not current working directory):
#   input  -> <repo>/.fex-emu/*.sqsh (latest by mtime)
#   output -> <repo>/.fex-emu/RootFS

usage() {
  cat <<'EOF'
Usage: prepare_fex_rootfs.sh [options]

Options:
  --input <path>    Path to .sqsh file. If omitted, auto-select latest from <repo>/.fex-emu
  --output <path>   Extraction output directory (default: <repo>/.fex-emu/RootFS)
  --clean           Remove output directory before extraction
  --help            Show this help
EOF
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing dependency: $1" >&2
    exit 1
  }
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
DEFAULT_INPUT_DIR="$REPO_ROOT/.fex-emu"
DEFAULT_OUTPUT_DIR="$REPO_ROOT/.fex-emu/RootFS"

INPUT_PATH=""
OUTPUT_DIR="$DEFAULT_OUTPUT_DIR"
CLEAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --input) INPUT_PATH="${2:?missing value}"; shift 2 ;;
    --output) OUTPUT_DIR="${2:?missing value}"; shift 2 ;;
    --clean) CLEAN=1; shift ;;
    --help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

require_cmd unsquashfs
require_cmd find
require_cmd sort

if [[ -z "$INPUT_PATH" ]]; then
  if [[ ! -d "$DEFAULT_INPUT_DIR" ]]; then
    echo "Input directory not found: $DEFAULT_INPUT_DIR" >&2
    echo "Run scripts/fetch_fex_rootfs.sh first or pass --input <file.sqsh>" >&2
    exit 1
  fi

  INPUT_PATH="$(
    find "$DEFAULT_INPUT_DIR" -maxdepth 1 -type f -name '*.sqsh' -printf '%T@ %p\n' \
      | sort -nr \
      | head -n1 \
      | cut -d' ' -f2-
  )"
fi

if [[ -z "$INPUT_PATH" ]]; then
  echo "No .sqsh file found in: $DEFAULT_INPUT_DIR" >&2
  exit 1
fi

if [[ ! -f "$INPUT_PATH" ]]; then
  echo "Input file not found: $INPUT_PATH" >&2
  exit 1
fi

if [[ "$INPUT_PATH" != *.sqsh ]]; then
  echo "Input must be a .sqsh file: $INPUT_PATH" >&2
  exit 1
fi

if [[ "$CLEAN" == "1" && -d "$OUTPUT_DIR" ]]; then
  rm -rf "$OUTPUT_DIR"
fi

mkdir -p "$OUTPUT_DIR"

echo "Input:  $INPUT_PATH"
echo "Output: $OUTPUT_DIR"
echo "Extracting..."
unsquashfs -f -d "$OUTPUT_DIR" "$INPUT_PATH"

if [[ ! -d "$OUTPUT_DIR/usr" ]]; then
  echo "Extraction finished but expected '$OUTPUT_DIR/usr' not found." >&2
  exit 1
fi

echo "RootFS ready: $OUTPUT_DIR"
