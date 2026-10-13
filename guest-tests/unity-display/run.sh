#!/usr/bin/env bash
# Linux regression test. Argument: prefix of a SDL2 build with Linux haptic/UDEV.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
prefix="${1:?pass the SDL2 install prefix}"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
cc -O2 -Wall -Wextra -Werror -shared -fPIC -Wl,-soname,libudev.so.1 \
  "$root/guest-tests/x11-fake/libudev_stub.c" \
  "$root/guest-tests/unity-display/fail_monitor.c" -Wl,--wrap=eventfd -o "$out/libudev.so.1"
ln -s libudev.so.1 "$out/libudev.so"
cc -Wall -Wextra -Werror "$root/guest-tests/unity-display/udev_contract.c" \
  -L"$out" -ludev -o "$out/udev_contract"
export LD_LIBRARY_PATH="$out:$prefix/lib:${LD_LIBRARY_PATH:-}"
"$out/udev_contract"
cc -Wall -Wextra -Werror "$root/guest-tests/unity-display/sdl_startup.c" \
  -I"$prefix/include/SDL2" -L"$prefix/lib" -lSDL2 -o "$out/sdl_startup"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
"$out/sdl_startup"
VEXA_TEST_FAIL_MONITOR=1 "$out/sdl_startup"
