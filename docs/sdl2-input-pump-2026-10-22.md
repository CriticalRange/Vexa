# SDL2 guest input pump — Warband fix (2026-10-22)

## Symptom

Mount & Blade: Warband (`steam-48700`) rendered fine but ignored all mouse
clicks and keyboard keys. Android-side logs (`VEXA-PAD`) showed events being
written to `/tmp/vexa-pointer`, and the file kept growing — nothing consumed it.

## Root cause

`mb_warband_linux` links `libSDL2-2.0.so.0` and reads input exclusively through
**state polling** (`SDL_GetMouseState`, `SDL_GetKeyboardState`,
`SDL_GetRelativeMouseState`, `SDL_GetModState`). It never calls
`SDL_PollEvent`/`SDL_PumpEvents`/`SDL_PeepEvents` on the frame path, so
`VexaSDL2PollPointerEvents()` was never invoked and the compat state
(`CompatMouseX/Y`, `CompatMouseButtons`, `CompatKeyStates`) never updated.

Verified live via `fexgdb` (qVexaThreads, stack dumps) and by appending records
to `/tmp/vexa-pointer` manually: no reaction until the fix.

## Fix — `ThunkLibs/libSDL2`

- `libSDL2_FNAStubs.inc`: `SDL_GetMouseState`, `SDL_GetRelativeMouseState`,
  `SDL_GetKeyboardState`, `SDL_GetModState` now call
  `VexaSDL2PollPointerEvents()` first (already throttled to 4 ms internally).
- `libSDL2_Guest.cpp`: on a key-down record for a printable key we now also
  queue an `SDL_TEXTINPUT` event — required by games that read names/text via
  SDL2 text entry instead of raw key events (Warband's name field).
- Diagnostics: one-shot open-failure log for `/tmp/vexa-pointer`; per-click
  transform trace gated behind `VEXA_SDL2_IN_TRACE=1`.

## Deploying thunk builds

```bash
# build (FEX android build tree)
cd FEX/build-android-arm64-ninja
ninja guest-libs            # -> Guest/libSDL2-guest.so etc.

# persist into the app runtime assets (ship in APK / installer)
cp Guest/libSDL2-guest.so \
   vexa/app/src/main/assets/runtime/thunks/guest/libSDL2-guest.so

# hot-deploy to a running device without reinstalling
adb push Guest/libSDL2-guest.so /data/local/tmp/
adb shell "run-as com.critical.vexaemulator sh -c \
  'cp /data/local/tmp/libSDL2-guest.so files/thunks/guest/libSDL2-guest.so'"
# then restart the guest (am force-stop + GameActivity --es programId <id>)
```

The device-side copy lives in
`files/thunks/guest/`; the runtime manifest pins sha256 of the installed libs.

## Injecting input records manually (debugging)

`/tmp/vexa-pointer` records are 16-byte little-endian `(type, x, y, state)`:
KeyPress=2, KeyRelease=3, ButtonPress=4, ButtonRelease=5, Motion=6, pad=7.
`x` for keys = X11 keysym. A click needs press and release in separate pump
drains (~100 ms apart) — state-polled games never see a same-drain down+up.
Coordinates are surface pixels, mapped to window coords via `/tmp/vexa-present`.
