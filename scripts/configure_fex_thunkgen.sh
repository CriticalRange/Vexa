#!/usr/bin/env bash
set -euo pipefail

FEX_ROOT="${FEX_ROOT:-/home/critical/FEX}"
BUILD_DIR="${BUILD_DIR:-$FEX_ROOT/build-host-thunkgen-ninja}"
LLVM_VER="${LLVM_VER:-19}"
LLVM_ROOT="${LLVM_ROOT:-}"

if [[ -z "$LLVM_ROOT" && -n "$LLVM_VER" ]]; then
  if [[ -d "/usr/lib/llvm${LLVM_VER}" ]]; then
    LLVM_ROOT="/usr/lib/llvm${LLVM_VER}"
  elif [[ -d "/usr/lib/llvm-${LLVM_VER}" ]]; then
    LLVM_ROOT="/usr/lib/llvm-${LLVM_VER}"
  else
    echo "[error] Could not find LLVM root for version ${LLVM_VER} under /usr/lib/llvm${LLVM_VER} or /usr/lib/llvm-${LLVM_VER}" >&2
    exit 1
  fi
fi

if [[ -n "$LLVM_ROOT" ]]; then
  CLANG_DIR="${CLANG_DIR:-$LLVM_ROOT/lib/cmake/clang}"
  LLVM_DIR="${LLVM_DIR:-$LLVM_ROOT/lib/cmake/llvm}"
  CC_BIN="${CC_BIN:-$LLVM_ROOT/bin/clang}"
  CXX_BIN="${CXX_BIN:-$LLVM_ROOT/bin/clang++}"
  CMAKE_PREFIX_PATH_VALUE="${CMAKE_PREFIX_PATH_VALUE:-$LLVM_ROOT/lib/cmake:/usr/lib/cmake}"
else
  CLANG_DIR="${CLANG_DIR:-/usr/lib/cmake/clang}"
  LLVM_DIR="${LLVM_DIR:-/usr/lib/cmake/llvm}"
  CC_BIN="${CC_BIN:-/usr/bin/clang}"
  CXX_BIN="${CXX_BIN:-/usr/bin/clang++}"
  CMAKE_PREFIX_PATH_VALUE="${CMAKE_PREFIX_PATH_VALUE:-/usr/lib/cmake}"
fi
OPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-/usr}"
OPENSSL_INCLUDE_DIR="${OPENSSL_INCLUDE_DIR:-/usr/include}"
OPENSSL_CRYPTO_LIBRARY="${OPENSSL_CRYPTO_LIBRARY:-/usr/lib/libcrypto.so.3}"
LIBXML2_INCLUDE_DIR="${LIBXML2_INCLUDE_DIR:-/usr/include/libxml2}"
CURL_INCLUDE_DIR="${CURL_INCLUDE_DIR:-/usr/include}"
CURL_LIBRARY="${CURL_LIBRARY:-/usr/lib/libcurl.so.4.8.0}"

if [[ ! -x "$CC_BIN" ]]; then
  echo "[error] Missing C compiler: $CC_BIN" >&2
  exit 1
fi
if [[ ! -x "$CXX_BIN" ]]; then
  echo "[error] Missing C++ compiler: $CXX_BIN" >&2
  exit 1
fi
if [[ ! -d "$CLANG_DIR" ]]; then
  echo "[error] Missing Clang CMake package dir: $CLANG_DIR" >&2
  exit 1
fi
if [[ ! -d "$LLVM_DIR" ]]; then
  echo "[error] Missing LLVM CMake package dir: $LLVM_DIR" >&2
  exit 1
fi

echo "[thunkgen] CC_BIN=$CC_BIN"
echo "[thunkgen] CXX_BIN=$CXX_BIN"
echo "[thunkgen] CLANG_DIR=$CLANG_DIR"
echo "[thunkgen] LLVM_DIR=$LLVM_DIR"

mkdir -p "$BUILD_DIR"

cmake -S "$FEX_ROOT" -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER="$CC_BIN" \
  -DCMAKE_CXX_COMPILER="$CXX_BIN" \
  -DClang_DIR="$CLANG_DIR" \
  -DLLVM_DIR="$LLVM_DIR" \
  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH_VALUE" \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=NEVER \
  -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" \
  -DOPENSSL_INCLUDE_DIR="$OPENSSL_INCLUDE_DIR" \
  -DOPENSSL_CRYPTO_LIBRARY="$OPENSSL_CRYPTO_LIBRARY" \
  -DLIBXML2_INCLUDE_DIR="$LIBXML2_INCLUDE_DIR" \
  -DCURL_INCLUDE_DIR="$CURL_INCLUDE_DIR" \
  -DCURL_LIBRARY="$CURL_LIBRARY" \
  -DFFI_INCLUDE_DIRS=/usr/include \
  -DFFI_LIBRARIES=/usr/lib/libffi.so \
  -DHAVE_FFI_CALL=1 \
  -DLibEdit_INCLUDE_DIRS=/usr/include \
  -DLibEdit_LIBRARIES=/usr/lib/libedit.so \
  -DHAVE_HISTEDIT_H=1 \
  -DBUILD_THUNKS=ON \
  -DENABLE_VULKAN_THUNKS=OFF \
  -DENABLE_X86_HOST_DEBUG=ON \
  -DBUILD_FEXCONFIG=OFF \
  -DBUILD_FEX_LINUX_TESTS=OFF \
  -DBUILD_TESTING=OFF \
  -DBUILD_STEAM_SUPPORT=OFF \
  -DENABLE_GDB_SYMBOLS=OFF \
  -DENABLE_LTO=OFF \
  -DENABLE_CCACHE=OFF \
  -DENABLE_WERROR=OFF \
  -DENABLE_JEMALLOC=OFF \
  -DENABLE_JEMALLOC_GLIBC_ALLOC=OFF

printf '\nConfigured thunkgen host build at: %s\n' "$BUILD_DIR"
printf 'Build thunkgen with:\n  cmake --build %q --target thunkgen -j\n' "$BUILD_DIR"
