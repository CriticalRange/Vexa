#!/usr/bin/env bash
set -euo pipefail

# Build x86_64 X11 compatibility stubs for Android guest runtime:
# - libX11.so.6
# - libxcb.so.1
# - libXau.so.6
# - libXdmcp.so.6

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/out"
CC_BIN="${CC:-gcc}"

mkdir -p "${OUTPUT_DIR}"

echo "[libx11-stub] CC=${CC_BIN}"
echo "[libx11-stub] output=${OUTPUT_DIR}"

echo "[libx11-stub] building libX11.so.6"
"${CC_BIN}" -shared -fPIC -nostdlib \
  -O2 -fno-stack-protector \
  -o "${OUTPUT_DIR}/libX11.so.6" \
  "${SCRIPT_DIR}/libx11_stub.c" \
  -Wl,-soname,libX11.so.6

echo "[libx11-stub] building libxcb.so.1"
echo 'void __vexa_libxcb_stub(void) {}' | \
  "${CC_BIN}" -shared -fPIC -nostdlib -x c - \
  -o "${OUTPUT_DIR}/libxcb.so.1" \
  -Wl,-soname,libxcb.so.1

echo "[libx11-stub] building libXau.so.6"
echo 'void __vexa_libxau_stub(void) {}' | \
  "${CC_BIN}" -shared -fPIC -nostdlib -x c - \
  -o "${OUTPUT_DIR}/libXau.so.6" \
  -Wl,-soname,libXau.so.6

echo "[libx11-stub] building libXdmcp.so.6"
echo 'void __vexa_libxdmcp_stub(void) {}' | \
  "${CC_BIN}" -shared -fPIC -nostdlib -x c - \
  -o "${OUTPUT_DIR}/libXdmcp.so.6" \
  -Wl,-soname,libXdmcp.so.6

ls -la "${OUTPUT_DIR}"/lib*.so*
