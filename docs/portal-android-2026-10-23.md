# Portal Linux on VEXA: installed, rendering blocked

## Verified result

Installed the supplied Portal package and added a `portal` custom-program entry.
Neither gameplay nor a rendered menu was reached. OpenGL initialization stops
at a missing required `glAlphaFunc` entry point. A separate `-vulkan` test stops
when SDL cannot load `libvulkan.so.1`; the device has no i386 Vulkan loader or
paired Vulkan guest/host thunks. These are current runtime capability gaps, not
evidence that the game's assets are missing or corrupt.

## Source and installation

- Source: `C:/Users/zeytu/Downloads/portal-linux/Portal (v.build 19017868 (27.06.25)).sh`
- SHA-256: `89fb4ea629353a609675883fe8baa3423e4a8150dc493fea87c96514632a65e8`
- The YAD installer wrapper was read, not executed. Its final 2,148,485,752 bytes
  are an XZ-compressed tar despite the wrapper's generic zstd comments.
- Decompressed tar stream: 4,565,872,640 bytes. Device tar exited successfully.
- Device game root: `/data/user/0/com.critical.vexaemulator/files/guests/portal`
- Original wrapper scripts are in `files/guests/portal-package`; the original
  downloaded installer remains untouched.
- `hl2_linux` is ELF32 i386, not x86_64. The bundled game includes `shaderapidx9.so`
  and `shaderapivk.so` (the latter depends on `libdxvk_d3d9.so`).

The custom entry launches `hl2_linux` from the game root with:

```
-game portal -novid -windowed -w 1280 -h 720 -nojoy -condebug +mat_antialias 0
```

Environment includes `DISPLAY=:0`, `SDL_VIDEODRIVER=x11`, `LD_LIBRARY_PATH=bin`,
`SteamAppId=400`, joystick/udev disables, `VEXA_X11_ABSOLUTE_POINTER=1`, and
`TMPDIR=/data/user/0/com.critical.vexaemulator/files/guests/portal/tmp`.
The Vulkan experiment's flag and LD_DEBUG tracing were removed from the saved
configuration. Balatro and Super Meat Boy records were preserved.

## Diagnosed and resolved launch dependencies

1. Initially `hl2_linux` printed `Failed to load the launcher`. `LD_DEBUG=libs`
   exposed an unresolved `gbm_bo_get_device` in the shared rootfs i386 SDL2.
   Copied the package's own
   `steam-runtime/i386/usr/lib/i386-linux-gnu/libSDL2-2.0.so.0` into this game's
   `bin/`. This SDL has only libc/libdl/libm/libpthread/librt direct dependencies.
   The launcher then loaded and initialized X11/GLX.
2. The launcher next printed `open(/tmp/source_engine_195990512.lock) failed`.
   Binary strings establish that it honors `TMPDIR`. Created the game's private
   `tmp/` and set TMPDIR accordingly. The engine progressed to graphics setup.

These changes are game-local. No global SDL replacement, shared rootfs change,
new binary patch, or FEX/VEXA source change was made for Portal. Pre-existing
Super Meat Boy changes remain separate. The game's i386 Steam library was not
replaced by VEXA's amd64 Steam stub.

## Remaining evidence and next implementation work

OpenGL produces a live GLES 3.2 surface and then exits with:

```
SDL failed to create GL compatibility profile (whichProfile=1!
Could not find required OpenGL entry point 'glAlphaFunc'!
```

The 32-bit GL guest resolver returns null when a function is absent from its
generated/compatibility tables. `glAlphaFunc` is currently absent. Valve's public
[ToGL source](https://github.com/ValveSoftware/ToGL) also declares this legacy
entry required and shows alpha-test state using it; merely returning a no-op
would not establish correct transparency or rendering. That public source is
older reference material, not a claim of an exact match to this shipped build.

Vulkan test output:

```
Failed to create SDL window: Failed loading libvulkan.so.1:
libvulkan.so.1: cannot open shared object file: No such file or directory
```

Device `files/thunks/guest_32` and `host_32` contain GL and OpenAL pairs only.
Current `FEX/ThunkLibs/HostLibs/CMakeLists.txt` explicitly excludes Android Vulkan
host builds for 32-bit guests. This route needs a paired i386 guest / arm64 host
Vulkan implementation and validation; copying the 64-bit guest library would
not work.

## Artifacts

`scratch/portal/` contains original and final custom-program JSON, extracted
launcher/engine/ToGL binaries for inspection, and separate `opengl-*` and
`vulkan-*` stderr/stdout/manifests. The original JSON backup is
`custom-programs.before.json`. The temporary 2 GB device transfer archive was
removed after installation. No APK was built or installed.

To undo the local SDL experiment, remove only the newly copied
`files/guests/portal/bin/libSDL2-2.0.so.0`; the package's Steam runtime copy is
unchanged. To remove the launcher entry, remove only `id=portal` from the current
JSON, preserving subsequent changes to other records.

## Follow-up: alpha testing implemented

The later implementation in [gl-alpha-test-android-2026-10-23.md](gl-alpha-test-android-2026-10-23.md)
adds opt-in alpha testing, passes 36 framebuffer pixel assertions on each of
i386 and x86_64, and has been packaged and installed in a new APK. Portal's
record now has `VEXA_GL_ALPHA_TEST=1`. The required `glAlphaFunc` check passes;
the next required entry point is `glAttachObjectARB`. The earlier statement
that no APK was built applies only to the initial installation investigation.

## Follow-up: ARB shader objects implemented

[arb-shader-objects-android-2026-10-23.md](arb-shader-objects-android-2026-10-23.md)
records the subsequent paired shader-object adapters and device tests. Portal
now advances past `glAttachObjectARB` and stops at `glBindProgramARB`, part of
the separate legacy assembly-program API. The updated APK is installed; Portal
still has no verified game rendering.

## Follow-up: ARB assembly program path

[arb-assembly-android-2026-10-23.md](arb-assembly-android-2026-10-23.md) records
the subsequent program namespace, parameter storage and bounded assembly-to-ESSL
path. Both guest ABIs pass 13 additional assembly framebuffer assertions, plus
the earlier regressions. Portal now passes `glBindProgramARB` and stops at the
next required function, `glClipPlane`. Portal gameplay is still unverified.


## 2026-10-24 follow-up

The clip-plane startup blocker and subsequent GL compatibility regressions are
implemented and device-tested in [the legacy GL follow-up](legacy-gl-portal-2026-10-24.md).
That report separates framebuffer probe coverage, menu rendering, first-map
validation and control acceptance.
