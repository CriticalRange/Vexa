#!/usr/bin/env bash
# Build guest X11 shims; GUEST_ARCH=i686 builds separate i386 outputs.
# Usage: scripts/build_x11_fakes.sh [name ...]   (default: all fakes present)
set -euo pipefail

NDK="${NDK:-C:/Users/zeytu/AppData/Local/Android/Sdk/ndk/30.0.16248370}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/guest-tests/x11-fake"
SYSROOT="$ROOT/guest-tests/graphics/rootfs"
GUEST_ARCH="${GUEST_ARCH:-x86_64}"
case "$GUEST_ARCH" in
  x86_64) OUTPUT="$SRC" ;;
  i686) OUTPUT="$SRC/i386" ;;
  *) printf 'unsupported GUEST_ARCH: %s\n' "$GUEST_ARCH" >&2; exit 1 ;;
esac
mkdir -p "$OUTPUT"
CLANG="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/x86_64-linux-android24-clang"

# fake source(s) -> produced soname.  Several fakes are assembled from more than
# one translation unit; dropping one silently removes exported symbols and makes
# SDL's dlsym table resolve to NULL, which reads as "x11 not available".
TARGETS=(
  "libX11_fake.c libX11_sdl_stubs.c:libX11.so.6"
  "libxcb_fake.c:libxcb.so.1"
  "libXrandr_fake.c libXrandr_sdl_stubs.c:libXrandr.so.2"
  "libXinerama_fake.c:libXinerama.so.1"
  "libXcursor_fake.c:libXcursor.so.1"
  "libXfixes_stub.c libXext_sdl_stubs.c:libXfixes.so.3"
  "libXi_fake.c libXi_sdl_stubs.c:libXi.so.6"
  "libXrender_fake.c:libXrender.so.1"
  "libXext_fake.c:libXext.so.6"
  "libXss_stub.c:libXss.so.1"
  "libXxf86vm_fake.c:libXxf86vm.so.1"
  "libxkbcommon_x11_fake.c:libxkbcommon-x11.so.0"
  "libdbus_fake.c:libdbus-1.so.3"
  "libgbm_stub.c:libgbm.so.1"
  "libudev_stub.c:libudev.so.1"
  "libpulse_stub.c:libpulse-simple.so.0"
  "libvexa_glmap.c:libvexa_glmap.so"
)

want=("$@")
for t in "${TARGETS[@]}"; do
  head="${t%%:*}"; so="${t##*:}"
  srcs=(); for s in $head; do srcs+=("$SRC/$s"); done
  if [ "${#want[@]}" -gt 0 ]; then
    match=0
    for w in "${want[@]}"; do
      for s in "${srcs[@]}"; do
        [ "$w" = "$so" ] || [ "$w" = "$(basename "$s")" ] || [ "$w" = "${so%%.so*}" ] && match=1
      done
    done
    [ "$match" = 1 ] || continue
  fi
  out="$OUTPUT/$so"
  if [ ! -x "$CLANG" ]; then
    CLANG="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/clang"
    "$CLANG" --target="$GUEST_ARCH-linux-gnu" -c /dev/null -o /dev/null 2>/dev/null || \
      CLANG="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/x86_64-linux-android-clang"
  fi
  "$CLANG" --target="$GUEST_ARCH-linux-gnu" --sysroot="$SYSROOT" -O1 -fPIC -shared \
           -fno-stack-protector -Wl,-soname,"$so" -I"$SRC" \
           -o "$out" "${srcs[@]}"
  printf 'built %-22s %8d bytes\n' "$so" "$(stat -c %s "$out")"
done