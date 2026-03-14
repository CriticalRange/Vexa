#!/usr/bin/env bash
set -euo pipefail

FEX_ROOT="${FEX_ROOT:-/home/critical/FEX}"
BUILD_DIR="${BUILD_DIR:-$FEX_ROOT/build-host-thunkgen-ninja}"
CLANG_DIR="${CLANG_DIR:-/usr/lib/cmake/clang}"
LLVM_DIR="${LLVM_DIR:-/usr/lib/cmake/llvm}"
CC_BIN="${CC_BIN:-/usr/bin/clang}"
CXX_BIN="${CXX_BIN:-/usr/bin/clang++}"
OPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-/usr}"
OPENSSL_INCLUDE_DIR="${OPENSSL_INCLUDE_DIR:-/usr/include}"
OPENSSL_CRYPTO_LIBRARY="${OPENSSL_CRYPTO_LIBRARY:-/usr/lib/libcrypto.so.3}"
LIBXML2_INCLUDE_DIR="${LIBXML2_INCLUDE_DIR:-/usr/include/libxml2}"
CURL_INCLUDE_DIR="${CURL_INCLUDE_DIR:-/usr/include}"
CURL_LIBRARY="${CURL_LIBRARY:-/usr/lib/libcurl.so.4.8.0}"

mkdir -p "$BUILD_DIR"

cmake -S "$FEX_ROOT" -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER="$CC_BIN" \
  -DCMAKE_CXX_COMPILER="$CXX_BIN" \
  -DClang_DIR="$CLANG_DIR" \
  -DLLVM_DIR="$LLVM_DIR" \
  -DCMAKE_PREFIX_PATH="/usr/lib/cmake" \
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
