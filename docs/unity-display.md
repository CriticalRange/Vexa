# Unity display initialization without private engine offsets

The runtime now fixes the Linux compatibility interfaces used by Unity's
embedded SDL, rather than calling private Unity functions. This applies to the
same API path regardless of Unity version or Mono versus IL2CPP. It does not
promise that every Unity release, rendering backend or shader works on Android.

## Failure chain

The old `libudev_stub.c` returned NULL from monitor and enumerator constructors.
These mean initialization failure, not an empty device list. SDL's Linux haptic
initialization fails when `SDL_UDEV_Init` fails. `SDL_InitSubSystem` then rolls
back subsystems it already initialized, including video. Unity can consequently
cache zero displays/resolutions even though later window creation succeeds.

This path is present in the locally inspected SDL2 source and Megabonk's
Unity 2023.2.22f1 player (ELF build ID
`b44d376601af15f44866ad622320c9a84dfea787`), which initializes SDL with flags
`0x3221` (timer, video, joystick, gamecontroller, haptic). Binary addresses were
used for diagnosis only; no engine addresses are used in the runtime.
The earlier on-device experiment proved that rebuilding the display list made
Megabonk render. The replacement must still receive on-device game validation.

## Changes

- Empty udev enumerators and reference-counted monitors initialize successfully.
  Monitors own quiet nonblocking, close-on-exec eventfds, cleaned up at the last
  unref. Empty receives return NULL/EAGAIN. Actual resource exhaustion still fails.
- Removed X11's Unity memory probes, private display refresh calls and resolution
  array mutation, plus the GL swap diagnostic that called private Unity functions.
- All three Android GLX context constructors use FEX's actual context registry.
  Failed MakeCurrent preserves guest current state. EGL configs explicitly support
  window and pbuffer surfaces for render/resource contexts.
- Bind and swap check Android surface generations without replacing the current
  guest context with the bootstrap context. Failed surface creation remains retryable.
- Rootfs/asset staging includes the udev shim and rejects a real host libudev.
  Runtime stamps exclude the stamp file itself, so identical assets stay identical.

Monitor constructor semantics: [upstream libudev source](https://github.com/systemd/systemd/blob/main/src/libudev/libudev-monitor.c).

## Validation and limits

`Unity display compatibility` CI builds SDL 2.0.22, 2.28.5 and 2.30.12 with Linux
haptic/UDEV. `guest-tests/unity-display/run.sh <SDL-install-prefix>` checks:

- 1,000 empty enumeration/monitor lifetime cycles, poll readiness, fd flags and
  cleanup, plus genuine descriptor exhaustion and recovery.
- 50 SDL startup/shutdown cycles using Unity's initialization flags: video stays
  initialized, desktop dimensions and display modes are nonzero, and no fds leak.
- A test-only failing monitor, with timer/video/haptic flags, reproduces the old
  SDL initialization error and confirms that SDL rolls video back. This checks
  the haptic failure independently of joystick cleanup (SDL 2.0.22 crashed when
  the full flags were used for this negative control). It also checks that SDL uses
  the tested udev path rather than silently bypassing it.

These Linux tests use SDL's dummy video driver. They exercise the startup failure,
not Android EGL, Unity's private SDL fork, or rendered game pixels. FEX GL host
(arm64 Android), GL guest (x86-64 Linux), X11 and udev shims must also compile.
No Android device was available for this change. Follow up on Megabonk and Vampire
Survivors with a fresh process, then test background/resume and another Unity
generation. Confirm nonempty displays and actual scene output; a successful build
or desktop-size log alone is insufficient. The existing tested launch-size presets
remain in place; native-first resolution mutation is intentionally removed.

## Shipping

Build `GL-guest` in FEX's guest build and `GL-host-64` in its Android build, then
stage **both** `libGL-guest.so` and `libGL-host.so` into `runtime/thunks` together.
The Gradle `stageSDL3Thunks` task now stages the GL pair as well and validates
all source files before copying. The shipped pair was built from FEX
`bd0a2bc18dedd993e22aedb15e5ef20b78329425`.
Build `scripts/build_x11_fakes.sh libX11.so.6 libudev.so.1`, copy those shims into
the staged rootfs and APK runtime rootfs, and regenerate `runtime/STAMP`.
Do not mix old and new thunk interfaces or restore the retired engine-offset patch.
