VEXA is a general-purpose Linux x86_64 runner. Older items that name one game are historical notes, not the current product.

## Runtime TODO: FEX Bring-up Sequence

- [x] First priority: Compile VEXA runtime wrapper directly against FEX headers/libs
  (`target_include_directories` + `target_link_libraries`) and avoid string-based `dlsym` calls
  for mangled C++ symbols.
- [x] Integrate `FEXCore::Config::Initialize()` in runtime startup and log success/failure as
  `FEX` category.
- [x] Integrate `FEXCore::Config::Load()` after config layering, with explicit error reporting if
  config paths are invalid.
- [-] Integrate `FEX::FetchHostFeatures()` and log key host feature flags used for runtime decisions
  (for example AVX and cache/atomic related flags) (partial: FetchHostFeatures is wired and
  SupportsAVX is
  used; host feature logging is still missing.).
- [x] Create FEX context with
  `FEXCore::Context::Context::CreateNewContext(HostFeatures)` and fail fast if context creation
  returns null.
- [-] Wire and set signal delegator (`CTX->SetSignalDelegator(...)`) and verify delegated signal
  path logging before guest execution. (wiring is done, signal-path verification logging is
  still missing.)
- [-] Wire and set syscall handler (`CTX->SetSyscallHandler(...)`) with structured logs for handler
  creation and attachment. (logging is needed for handler created/instance info and handler attached
  to context confirmation)
- [-] Call `CTX->InitCore()` and treat non-success as a hard startup failure with dedicated failure
  code and diagnostics. (detailed diagnostics/cause fields are missing.)
- [-] Add initial thread lifecycle bring-up (`CreateThread` + `ExecuteThread`) behind preflight
  success, with guarded logging around enter/exit and crash boundaries. (CreateThread lifecycle
  logging (create/track/register/teardown) is still minimal.)
- [ ] Add a runtime snapshot API (read-only state/progress object) that exposes
  preflight/init/execute
  stage completion flags and last failure `Code/Phase/Reason`, so Kotlin/UI and dependent subsystems
  can gate behavior without parsing logs.
- [-] Design and implement supervisor/worker split: keep `RuntimeService` as supervisor in
  `:runtime` (partial: supervisor/worker split and Messenger forwarding are implemented across :
  runtime and :runtime_worker; worker death is detected and state reset, but full automatic
  rebind/restart + in-flight request recovery is still incomplete.)
  and move native/FEX execution to a dedicated worker process (for example `:runtime_worker`), with
  Binder/Messenger forwarding and worker-death recovery.
- [-] Add cross-process surface transport for worker mode: define explicit IPC for `Surface` handoff
  (Parcelable/Binder path), ownership lifecycle, and teardown semantics so worker-render path can
  consume the UI-created surface safely. (resize/change control path still has TODO
  and is not fully wired end-to-end.)
- [ ] Add latency budget and transport plan for high-frequency input in worker mode
  (`UI -> supervisor -> worker`), with criteria for when to keep Binder and when to switch to
  shared memory or socket-based fast path.
- [-] Add worker-process file/FD permission validation for game/rootfs/thunk/artifact paths and any
  URI-backed inputs: ensure grants are applied before worker start and log explicit
  permission-denied
  diagnostics with failing path. (partial: worker-side preflight validates basic
  filesystem paths before launch, but URI/FD grant
  validation is not implemented and permission-denied
  diagnostics do not yet include explicit failing path +
  errno/details.)
- [ ] Fix `:runtime_worker` stack-bound crash seen in logcat on 2026-03-18
  (`thread.cc:1489 Check failed: FindStackTop<stack_type>() >
  reinterpret_cast<void*>(GetStackEnd<stack_type>())`), which is followed by
  `signal 11 (Segmentation fault)` worker exits.
- [x] Ensure all FEX entry points (`SetSignalDelegator`, `CreateThread`, `ExecuteThread`) run on a
  dedicated native worker pthread, never on Binder/Java framework threads.
- [x] Add explicit pthread stack policy for FEX worker threads
  (`pthread_attr_setstacksize` + `pthread_attr_setguardsize`) and document/enforce a minimum safe
  stack size for Android worker mode.
- [-] Add stack preflight diagnostics before entering FEX:
  `pthread_getattr_np` + `pthread_attr_getstack` logging/validation for every thread that may enter
  FEX; fail fast with a clear `THREAD`/`SIGNAL` reason when bounds are invalid. ((partial:
  runtime_worker now probes pthread stack via
  pthread_getattr_np/pthread_attr_getstack, but bounds
  are not validated/enforced, failures are not fail-fast,
  and no dedicated THREAD/SIGNAL failure code is emitted
  for invalid bounds.))
- [-] Track and patch FEX `/proc/pid/cmdline` remap behavior on Android app UIDs:
  `prctl(PR_SET_MM, PR_SET_MM_MAP, ...)` fails without privileged caps (expected, non-fatal).
  Action: downgrade to warn-once or skip on Android, and keep launch flow unaffected. ((partial:
  behavior is tracked and currently non-fatal,
  but remap still logs hard errors on Android; needs
  Android skip or warn-once downgrade for PR_SET_MM_MAP
  capability failure.))
- [-] Add allocator parity follow-up for 32-bit mode: after `SetupHooks()` +
  `CreatePassthroughAllocator()`, mirror interpreter behavior that drains pre-reserved host
  allocator
  space (the malloc loop) before guest execution. ((partial: SetupHooks + CreatePassthroughAllocator
  are
  in place for non-64 path, but interpreter-style pre-reserved allocator drain loop is not
  implemented in
  VEXA yet; 32-bit mode is currently forced off by
  CONFIG_IS64BIT_MODE=1.))

## Required Thunks Fixes

- [x] Fix the guest X11/RandR fakes so a Unity guest sees a real display.
  Problem 1: `XOpenDisplay` in `guest-tests/x11-fake/libX11_fake.c` did
  `calloc(1, sizeof(Display))`, but `Display` is a pointer typedef, so it allocated 8 bytes
  and wrote every Xlib field past the end. Unity reads the screen through the
  `ScreenOfDisplay(dpy, 0)->width_in_pixels` macro and saw garbage, logging
  `Desktop is 0 x 0 @ 0 Hz` and `Error getting num native displays: Video subsystem has not
  been initialized`, then rendering nothing. Fixed with `sizeof(*dpy)` (4720 bytes).
  Problem 2: `guest-tests/x11-fake/libXrandr_fake.c` hand-declared the Xrandr.h wire structs and
  two of them had drifted. `XRRModeInfo` must declare `hSyncStart`..`vTotal` as `unsigned int`;
  declaring them `unsigned short` shifts `name`, `nameLength` and `modeFlags` and made Unity
  compute the refresh rate as 0 Hz for a mode whose `dotClock/(hTotal*vTotal)` is exactly 60.
  `XRRCrtcInfo` must be `timestamp, x, y, width, height, mode, rotation, noutput, outputs,
  rotations, npossible, possible` and has no `nmode`/`modes`. Both now come verbatim from a
  vendored copy of the real header, `guest-tests/x11-fake/Xrandr_fake_types.h`, with
  `offsetof` assertions so a future drift fails the build.
  Verified: Unity now logs `Desktop is 2712 x 1220 @ 60 Hz` and creates a real OpenGL 4.3 device.
  Note for future builds: the first launch after an APK install re-stages the rootfs from the
  APK and silently overwrites any guest library that was pushed with `adb`. Always launch once,
  then push, then launch again.

- [ ] Unity 2022+ renders an empty scene on the GLX path (Megabonk, VampireSurvivors).

  Problem: Unity completes `GfxDevice` init, logs `Renderer: Mali-G720 / Version 4.3.0` and
  `Desktop is 2712 x 1220`, compiles ~56 shader programs (all link `status=1`), loads ~22k
  objects and runs the game's C# (Steam, Rewired, saves, settings). Yet each frame issues only
  two fullscreen blits: `bind fbo4 -> clear(0x4500) -> draw 6 verts`, `bind fbo2 ->
  clear(0x4000) -> draw 6 verts`, `swap`. No geometry draw is ever recorded, and both targets
  are 256x256 (`glTexStorage2D 256x256 fmt=0x8c43 levels=9`), while the X11 window and
  `glXQueryDrawable` both correctly report 2712x1220.
  Ruled out: display/desktop size (fixed, see below), `GLX_MAX_BUFFER_SIZE` (Unity never calls
  `glXGetFBConfigAttrib`), `GL_MAX_*` limits (none near 256), `-screen-width/height` (ignored,
  1280x720 and 2712x1220 behave identically), the 32x32 X11 probe window, and advertising or
  hiding `GL_ARB_compute_shader` (the game's `boot.config` sets
  `gfx-enable-gfx-jobs=1 / gfx-enable-native-gfx-jobs=1`, but removing the spoof changed nothing).
  Method: the 256x256 backbuffer is chosen by Unity before any GL call, so trace it from the
  engine side next â€” dump the resolved `Screen.width/height` and the URP
  `UniversalRenderPipelineAsset` render scale / `m_UseDynamicResolution` at device creation, and
  check whether `ScalableBufferManager.ResizeBuffers` is ever invoked. Celeste (Unity 2019) is
  unaffected because it renders straight to FB0, which makes this an offscreen-backbuffer path
  issue rather than a general Unity culling issue.
  Strongest lead so far: the game never gets past `LocalizationPrewarmer:Start()` and logs
  `IndexOutOfRangeException` from `CurrentSettings.DoUpdateResolution`, whose saved
  `cfVideoSettings.resolution` is `0`. That means `Screen.resolutions` comes back empty, and an
  empty resolution list also makes `Screen.SetResolution` never complete, which would stall the
  boot coroutine before the menu scene loads and leave an empty scene behind. Forcing
  `fullscreen_mode: 0` in the save did not change anything, and filling the CRTC `possible` mode
  list did not populate the array either.

## Screen.resolutions is empty - root cause found (2026-10-12)

Traced end to end with IL2CPP decompilation plus a live memory probe. This is a
*separate* bug from the empty-scene issue above and it is now fully explained.

### The game's crash

`Il2CppDumper 6.7.46` on `GameAssembly.so` + `global-metadata.dat` gives
`Assets.Scripts.Settings___Saves.SaveFiles.ConfigSettingsUtility::GetMyResolutions()`
at RVA `0x2229400` and `CurrentSettings::SetResolution(int)` at `0x2196D40`.
Decompiled:

```csharp
// ConfigSettingsUtility::GetMyResolutions  (0x2229400)
var list = new List<Resolution>();
foreach (var r in Screen.get_resolutions())            // UnityEngine.Screen$$get_resolutions
    if (!list.Any(e => e.width == r.width && e.height == r.height))
        list.Add(r);
return list.ToArray();                                 // empty in, empty out

// CurrentSettings::SetResolution  (0x2196D40)
var res = ConfigSettingsUtility.GetMyResolutions();
if ((uint)index >= res.Length) throw new IndexOutOfRangeException();   // 0 >= 0
Screen.SetResolution(res[index].width, res[index].height, refreshRate, fullscreenMode);
```

and `Player.log` matches exactly:

```
IndexOutOfRangeException: Index was outside the bounds of the array.
  at CurrentSettings+<DoUpdateResolution>d__17.MoveNext ()
  at CurrentSettings.OnSettingUpdated (System.String name, System.Object value, System.Object oldValue)
  at CurrentSettings.UpdateSave ()
```

So the whole chain reduces to one question: why is `Screen.resolutions` empty.

### Why: SDL reports a display with zero modes

`UnityPlayer.so` is stripped but `get_resolutions_Injected` is registered in the
scripting table at file offset `0x92e8`; its implementation builds the array at
`0xde2700`:

```c
// 0xde2700  DisplayManager::GetAvailableResolutions
for (int d = 0; ; d++) {
    int n = SDL_GetNumVideoDisplays();                       // 0x18b2ee7
    if (n <= 0) { Log("Error getting num native displays: %s", SDL_GetError()); break; }
    if (d >= n) break;
    Entry* e = push_new_entry(d);
    if (!GetDisplayInfo(d, e)) break;                         // 0xde2640
}
```

`0xde2640` calls `SDL_GetNumDisplayModes` (`0x18b2f9a`), which simply returns
`display->num_display_modes`, and `SDL_GetDisplayMode`-style helpers that copy
`display + 0x18`.

`libX11_fake.c` now probes SDL's live state from inside the guest: it resolves the
`UnityPlayer.so` load bias with `dladdr(__builtin_return_address(0))` and reads
SDL's `_this` (vaddr `0x1fb5138`, `num_displays` at `+0x2d0`, `displays` at `+0x2d8`,
stride `0x78`). Output:

```
probe(XCreateWindow) base=0x7a135d0000 _this=0x481600
num_displays=1 displays=0x431c00
display[0] raw: +0x00 20 38 48 00 ...        <- driverdata
                +0x18 03 18 40 17 98 0a ...  <- desktop_mode  2712x1220 @60   (correct)
                +0x38 03 18 40 17 98 0a ...  <- current_mode  2712x1220 @60   (correct)
display_modes=0x483820 num_display_modes=0                                <- BUG
```

The display exists and carries the right size, but its **mode list is empty**, so
Unity produces a zero-entry `Screen.resolutions`.

### SDL's mode enumeration never runs

The RANDR fake now logs `__builtin_return_address(0)` on every call, and the whole
X11 init sequence is:

```
XRRQueryVersion -> XRRGetOutputPrimary -> XRRGetScreenResourcesCurrent
  -> XRRGetOutputInfo -> XRRGetCrtcInfo -> XRRGetCrtcInfo
  -> XRRGetScreenResourcesCurrent        (second looking_for_primary pass)
```

That is exactly SDL2's `X11_InitModes_XRandR`: two passes over
`looking_for_primary = 1, 0`, one `XRRGetOutputInfo` from `X11_AddXRandRDisplay`,
and the second `XRRGetCrtcInfo` from `SetXRandRModeInfo`.

**SDL's `X11_GetDisplayModes` is never entered** - it would issue its own
`XRRGetScreenResources` + `XRRGetOutputInfo` pair, and neither appears. Xinerama
and XF86VidMode are not consulted either (both fakes now log; neither fires).
Unity's bundled SDL therefore relies on modes being filled during
`SDL_VideoInit`, and that pass adds nothing.

Not ruled out yet, and the next thing to check:

- whether Unity's SDL is SDL3 rather than SDL2 (the mode struct carries a
  `pixel_density` double, which is SDL3-only). In SDL3 `SDL_GetNumDisplayModes`
  is documented to be non-lazy as well, so the missing step is the same either way.
- whether `X11_AddXRandRDisplay` returned before reaching `SDL_AddVideoDisplay` in
  a path that still registered the display - the desktop mode is set, so it did
  run; the open question is only who was supposed to call `GetDisplayModes`.

### Tooling added

- `scripts/build_x11_fakes.sh` - builds every guest X11 fake for x86_64 against
  `guest-tests/graphics/rootfs`. Several sonames need more than one translation
  unit (`libX11.so.6` = `libX11_fake.c` + `libX11_sdl_stubs.c`); dropping one
  silently removes exported symbols and SDL reports "x11 not available".
- `scripts/grab_log.ps1` - `adb logcat` capture. PowerShell's `>` writes UTF-16,
  which silently corrupts logs.
- `guest-tests/x11-fake/libXxf86vm_fake.c` - real `libXxf86vm.so.1` replacing the
  all-NULL stub (kept because SDL consults it when XRandR is unavailable).
- `libXrandr_fake.c` `VEXA_RANDR_TRACE` now logs the guest return address so
  call sites can be located in the stripped engine.
- `libX11_fake.c` `vexa_probe_sdl()` - reads SDL's live display list from inside
  the guest via `dladdr` + known offsets.

- [ ] Stabilize SDL3 host thunk lifecycle in
  `FEX/ThunkLibs/libSDL3/libSDL3_Host.cpp`.
  Problem: managed EGL state (`TryCreateManagedEGLContext`/`RebuildManagedSurfaceFromRuntimeWindow`)
  can churn between pbuffer and runtime window and is sensitive to thread ownership
  (`ValidateGLOwnerThread`).
  Method: add deterministic state machine logs with transition IDs, enforce single owner-thread
  policy at API boundary, and run create/destroy/relaunch stress with forced surface-serial
  changes.
- [ ] Reduce SDL3 guest behavior drift in
  `FEX/ThunkLibs/libSDL3/libSDL3_Guest.cpp`.
  Problem: large synthetic behavior layer (`SyntheticEventQueue`, property store,
  fallback window/display values) can diverge from SDL3 semantics.
  Method: build a conformance checklist for event order/window flags/properties, compare against
  native SDL3 traces, and gate synthetic events behind explicit conditions instead of
  unconditional pushes.
- [ ] Validate `glXGetProcAddress` routing in
  `FEX/ThunkLibs/libGL/libGL_Host.cpp`.
  Problem: lookup path spans wrapper hits, shader bridge, `eglGetProcAddress`, and `dlsym`,
  which risks nondeterministic proc resolution.
  Method: codify a strict precedence table and add a lookup audit log mode that records proc name,
  chosen source, and pointer hash for reproducibility.
- [ ] Investigate per-frame glViewport alternation in
  `FEX/ThunkLibs/libGL/libGL_Host.cpp`.
  Problem: Godot (Brotato) and FNA3D (Celeste) each issue two glViewport calls per frame â€”
  full surface `0,0 2712x1220` then a narrower centered `272,0 2168x1220` â€” so the host remap
  path toggles the visible rect every frame and produces a visible "flicker"/shimmer of splash
  and UI elements. Both games stay stable at ~120 fps, so this is cosmetic, but the alternating
  viewport is a rendering-correctness smell.
  Method: capture the glViewport call sequence with caller context (draw/read FBO binding),
  determine whether the narrow rect is a genuine guest off-screen pass, and either suppress the
  remap for the matching case or record why the guest alternates.
- [ ] Harden Android guest GL proc handling in
  `FEX/ThunkLibs/libGL/libGL_Guest.cpp`.
  Problem: unresolved proc path currently returns `MissingGLProcStub` (fatal) and alias/compat
  maps are manual (`ResolveGLInvokerAlias`, `ResolveAndroidCompatGuestProc`).
  Method: add allowlisted soft-fail behavior for non-critical probes, generate alias coverage tests
  from known title call patterns, and promote fatal only for draw-path essentials.
- [ ] Re-audit global thunkgen behavior in
  `FEX/ThunkLibs/Generator/gen.cpp`.
  Problem: generator now injects null-target traps and Android relaxed init exceptions for specific
  libraries; this can hide regressions or over-apply policy.
  Method: split generator policy by explicit config flags per library, add generated-code golden
  tests for `libGL`/`libSDL3`, and document when relaxed init is allowed.
- [ ] Review fatal guards in `FEX/ThunkLibs/include/common/Host.h` and
  `FEX/ThunkLibs/include/common/Guest.h`.
  Problem: many paths now `abort`/`trap` immediately, which is good for diagnosis but harsh for
  runtime stability.
  Method: classify each guard as debug-only vs always-fatal, add one-line reason code mapping, and
  keep release path fail-fast only for memory safety/callback corruption cases.
- [ ] Lock down Android host build wiring in
  `FEX/ThunkLibs/HostLibs/CMakeLists.txt`.
  Problem: runtime shader translator defaults ON and hard-fails when `VEXA_THIRD_PARTY_DIR`
  is missing.
  Method: switch to opt-in or auto-disable with warning, and print one consolidated build summary
  showing enabled thunk features and dependency status.
- [ ] Keep GL thunkgen surface synchronized between
  `FEX/ThunkLibs/libGL/libGL_interface.cpp` and `FEX/ThunkLibs/libGL/android_gl_proc_list.h`.
  Problem: proc allowlist is consumed in multiple places (GL interface + SDL guest table), so
  drift is easy.
  Method: add a CI script that extracts proc names from each source and fails on missing/extra
  entries.
- [ ] Verify `BUILD_ANDROID=1` propagation boundaries in
  `FEX/CMakeLists.txt` and `FEX/ThunkLibs/GuestLibs/CMakeLists.txt`.
  Problem: macro is forced globally and in external guest builds, risking accidental Android path
  selection outside intended targets.
  Method: constrain define injection to thunk targets only and add compile-time banner logs proving
  active target mode.
- [ ] Confirm SDL3 bridge symbol contract in
  `FEX/ThunkLibs/libSDL3/libSDL3_interface.cpp`.
  Problem: interface/host contract is large and regression-prone even when currently matching.
  Method: generate interface-vs-host symbol diff in CI (declared `FEX_SDL_*` vs implemented
  `fexfn_impl_libSDL3_FEX_SDL_*`) and fail on mismatch.
- [ ] Re-validate EGL Android compatibility glue in
  `FEX/ThunkLibs/libEGL/libEGL_Guest.cpp` and `FEX/ThunkLibs/libEGL/libEGL_Host.cpp`.
  Problem: Android path depends on forward-declared `glXGetProcAddress` and
  `EGLNativeWindowType` guest->host layout conversion.
  Method: run 32/64-bit handle conversion smoke tests and verify context creation/swap on at least
  two Android GPU stacks.
- [ ] Complete OpenAL thunk validation for expanded surface in
  `FEX/ThunkLibs/libopenal/libopenal_interface.cpp`.
  Problem: interface surface grew significantly but runtime path still depends on generated host
  symbol resolution quality.
  Method: run OpenAL API smoke matrix (`alcOpenDevice`, context create/make current,
  source/buffer play/stop) and capture missing-symbol/load diagnostics in startup logs.

## SDL3 Thunk TODO (FEX-Style, Android)

- [-] Define scope/version: choose target SDL3
  SONAME(s) and exact API surface to thunk first
  (minimal boot set, then expand). (partial: scope is
  currently implied by `libSDL3.so` thunk SONAME
  wiring and deploy symbol gates, but there is no
  explicit version-pinned scope document yet.)
- [x] Include `SDL3_image` in first-pass scope
  planning: Hytale references both `SDL3` and
  `SDL3_image`, so startup path must cover both
  libraries (thunk or compatible guest-lib strategy).
- [x] Create upstream thunk library skeleton:
  `ThunkLibs/libSDL3/` with `libSDL3_interface.cpp`,
  `libSDL3_Guest.cpp`, `libSDL3_Host.cpp`.
- [-] Create companion plan for `libSDL3_image`
  (`ThunkLibs/libSDL3_image/` or explicit non-thunk
  strategy) and define dependency/link order relative
  to `libSDL3`. (partial: `libSDL3_image` thunk is
  implemented and wired, but explicit dependency/link
  ordering policy is not documented via DB `Depends`
  or equivalent contract text.)
- [-] Implement generator config (`fex_gen_config`,
  `fex_gen_type`, `fex_gen_param`) for SDL3 types/
  functions, including callback strategy and pointer/
  opaque annotations. (partial: `fex_gen_config` +
  `fex_gen_type` with opaque/custom-host coverage are
  in place; callback strategy and `fex_gen_param`
  coverage are not fully explicit/validated yet.)
- [x] Add build wiring in FEX CMake (`ThunkLibs/
  GuestLibs/CMakeLists.txt` and `ThunkLibs/HostLibs/
  CMakeLists.txt`) for `libSDL3` guest/host thunk
  targets.
- [-] Add thunk DB mapping in `Data/ThunksDB.json`
  for SDL3 overlays (`Library`, `Overlay`, optional
  `Depends`) so guest SDL3 resolves to `libSDL3-
  guest.so`. (partial: upstream `Data/ThunksDB.json`
  still lacks SDL3 entries; VEXA currently generates
  SDL3/SDL3_image/openal mappings dynamically in
  runtime `SetupConfig`.)
- [x] Add/verify app config enablement (`ThunksDB`
  key for SDL3) so runtime actually turns SDL3
  thunking on.
- [-] Ensure Android runtime paths are correct:
  `THUNKHOSTLIBS` and `THUNKGUESTLIBS` contain SDL3
  thunk artifacts and are preflight-validated.
  (partial: paths are wired and thunk directories are
  preflight-checked, but preflight does not yet
  enforce required per-file artifact presence.)
- [-] Resolve host SDL3 load path on Android
  (`dlopen` target used by generated host loader) and
  verify dependencies are present. (partial:
  host-loader path + Android host alias mapping are
  implemented, but end-to-end dlopen dependency
  validation remains mostly script/manual.)
- [-] Extend deploy scripts to push SDL3 host/guest
  thunk artifacts and verify on-device presence before
  launch. (partial: push script deploys artifacts and
  shows remote summaries, but strict fail-gated remote
  verification coverage can still be tightened.)
- [-] Add structured logs for thunk load lifecycle
  (`fex:loadlib`, host export init, symbol
  registration, overlay hit/miss). (partial: `LoadLib`
  dlopen/dlsym diagnostics exist, but lifecycle logging
  is not yet a fully structured end-to-end event set.)
- [-] Validate callback round-trip
  (guest->host->guest) with canary SDL3 callback
  paths. (partial: callback trampoline plumbing exists
  and SDL callback usage appears in thunk code, but no
  explicit SDL3 callback canary validation is
  documented.)
- [ ] Run incremental test matrix: startup, window
  init, input, audio, teardown, relaunch stability.
- [x] Redo libalsa support. The generated desktop
  `libasound` thunk stays disabled on Android. Guests
  that dlopen `libasound.so.2` (Godot ALSA) get a new
  guest library: the symbol surface the loader expects,
  with playback forwarded through the OpenAL thunk to
  host `openal-soft` (OpenSL). Pulse is left absent so
  the title falls through to this ALSA entry.
- [-] Android audio backend plan: keep desktop
  `libasound` thunks disabled in Android builds and
  use `openal-soft` as primary audio replacement;
  keep SDL3 Android audio backend (`AAudio` /
  `OpenSLES`) as fallback path when title uses SDL
  audio directly. (partial: Android builds disable
  ALSA thunk paths and openal-soft Android backends
  are configured; fallback policy/testing for direct
  SDL audio path is not fully codified.)
- [x] Add explicit OpenAL thunk track:
  define `openal` SONAME mapping, thunk surface, and
  host load behavior for Android (`openal-soft`).
- [ ] Disable OpenAL real-time thread priority on Android:
  add an `alsoftrc` override with `rt-prio = 0` and
  wire `ALSOFT_CONF` in runtime env setup so
  `[ALSOFT] pthread_setschedparam failed: Operation not permitted`
  is suppressed without regressing audio output.
- [ ] Define rendering ownership contract before
  broad thunk expansion:
  `SDL3` video path vs `libGL/libEGL` redirect path,
  and expected handoff points between them.
- [x] Document known Android-specific deviations and
  upstream patches under `VEXA_FIXES` comments where
  needed.

## libc Compatibility Scope (Android Bring-up)

- [ ] Do not start with full libc thunk:
  keep rootfs glibc + FEX syscall emulation as default
  path, and only add thunked compat for proven gaps.
- [ ] Collect concrete missing-libc evidence first:
  capture unresolved symbols / runtime failures from
  logs (`fex_stderr`, loader diagnostics) and build a
  prioritized minimal API list.
- [ ] Split libc needs by responsibility:
  process/thread/time/fs/memory APIs belong to libc/
  syscall compatibility, while rendering stays in SDL/
  GL/EGL layers.
- [ ] If libc thunking is required, implement narrow
  compatibility overlays in phases (boot blockers
  first), with one canary function group at a time and
  rollback-safe toggles.
- [ ] Add explicit test gates for libc-compat changes:
  startup, thread lifecycle, memory alloc/free stress,
  and relaunch stability before widening API surface.

## libGL -> GLES Redirect Findings (Android)

- [-] Preserve `libGL` thunk ABI surface first: current
  `ThunkLibs/libGL/libGL_interface.cpp` is heavily
  `glX*` and X11-oriented, so replacement must keep
  expected entrypoints/signatures even if internals
  redirect. (partial: ABI-facing `glX*` surface is
  still present with Android compatibility declarations,
  but there is no explicit symbol/ABI regression gate
  yet.)
- [x] Keep `glXGetProcAddress*` compatibility behavior:
  `libEGL` guest thunk currently relies on
  `glXGetProcAddress` path (`ThunkLibs/libEGL/
  libEGL_Guest.cpp`), so redirect layer must still
  provide this contract.
- [-] Preserve guest `libGL` init side effects:
  `libGL_Guest.cpp` wires callback hooks and implicit
  X11 dependency assumptions (`XSync`,
  `XGetVisualInfo`, `XDisplayString`, `libX11` pull-in).
  (partial: desktop path keeps callback wiring, but
  Android path intentionally skips X11 callback wiring
  and uses no-op X11 bridge handlers.)
- [-] Keep SONAME/overlay identity unchanged:
  `Data/ThunksDB.json` overlay mapping for `libGL.so*`
  -> `libGL-guest.so` must remain stable while internals
  are replaced. (partial: upstream `Data/ThunksDB.json`
  keeps stable `libGL.so*` mapping, but VEXA runtime
  generated thunk DB currently omits GL/EGL entries.)
- [-] Keep guest link graph valid:
  `ThunkLibs/GuestLibs/CMakeLists.txt` links
  `EGL-guest` to `GL-guest` and injects placeholder
  `libX11.so.6`; update only after confirming no symbol
  breakage. (partial: current link graph still matches
  this contract, but no explicit symbol-breakage gate is
  recorded.)
- [x] Gate desktop OpenGL host thunk on Android:
  current host build path requires desktop OpenGL/GLX
  (`find_package(OpenGL REQUIRED)`), so Android build
  must skip desktop `libGL` host thunk until redirect
  host implementation is ready. (done via Android host
  path linking against `EGL/GLES` while desktop OpenGL
  link remains non-Android only.)
- [-] Implement Android host redirect layer in phases:
  phase 1 = compatibility shim + stubs, phase 2 = map
  viable calls to `EGL/GLES`, phase 3 = remove stubs
  only after runtime validation. (partial: shim/stub +
  initial GLES mapping are implemented in `libGL`
  guest/host paths, but staged phase completion and
  stub-removal validation are not complete.)

## Hytale Integration

- [x] Make "Login to Hytale" button functional.
- [ ] Implement CDN game assets retrieval

## Steam libraries

- [x] For now, place androidarm64 `libsteamclient.so` on the device only.
  It is taken from GameNative's `steam-androidarm64-20260709.tzst`
  (`downloads.gamenative.app`) and copied into the app-private
  `files/steam/androidarm64/` directory. It is not in the git repo.
  `libsteam_api.so` in that directory is the separate file from the
  user's Steamworks SDK 1.65 zip. Do not vendor either `.so`, and do
  not copy GameNative's `libsteambootstrap.so`.
- [ ] Replace that borrowed `libsteamclient.so`. The copied file does
  not by itself make a guest `SteamAPI_Init` pipe succeed.
- [x] Do not run the Linux Steam client under FEX, and do not start a
  second FEX instance for it. Valve's listener is `ubuntu12_32/steam`
  (32-bit) and it execs 64-bit helpers such as steamwebhelper. One FEX
  process has one guest bitness: the allocator and the syscall handler
  are process-global. VEXA forces `CONFIG_IS64BIT_MODE=1`, creates only
  `FEX::HLE::x64::CreateHandler`, builds Android thunks for 64-bit
  only, and `execute.cpp` rejects a bitness mismatch. Upstream 32-bit
  support is not wired here. Guest exec across bitness is also absent
  (`VEXA_DISABLE_FEXSERVER=1`). A second process would still be the
  32-bit client and would still need further processes for its
  helpers. Refresh tokens stay in the keystore. Do not write one into
  the guest tree, do not log it, do not set `Steam3Master` without a
  listener, and do not fake `CreateSteamPipe`.
- [x] Do not build an out-of-FEX Steam host from the libraries we have.
  The game stays the one 64-bit FEX guest. A listener would be a normal
  ARM64 process that logs in with the VEXA refresh token and opens the
  local pipe. `libsteam_api.so` `SteamAPI_Init` and `libsteamclient.so`
  `CreateInterface` join a running host. They do not start one.
  JavaSteam speaks to Steam's network, not that local pipe. The only
  program known to start the host is GameNative's closed
  `libsteambootstrap.so`. Do not copy or reverse it. Do not write a
  replacement pipe server.

## Unity / IL2CPP Linux player

- [x] Make Unity build its display list. Unity asks for displays before SDL
  video is up, caches the empty answer and never asks again. Two failures
  follow: `Screen.resolutions` comes back empty (games that restore a saved
  resolution index die with `IndexOutOfRangeException` before loading a
  scene) and with no display there is no camera, so the scene never
  renders. `vexa_unity_fixup()` in `libX11_fake.c` calls the refresh at
  `UnityPlayer.so+0x15ea030` and the enumeration at `+0xde04c0` again once
  the window exists. Both are guarded by an instruction-byte signature, so
  a different Unity build skips them instead of jumping into the wrong
  function.
- [x] Drop the launcher shim. `-force-gfx-direct` and `-force-glcore` are
  passed as ordinary arguments from `GuestProgram.kt`; the real
  `Megabonk.x86_64` stub stays in place. `boot.config` asks for
  `gfx-enable-gfx-jobs=1` / `gfx-threading-mode=4`, which selects
  `kGfxThreadingModeClientWorkerJobs`; the job worker produces no output
  here and the scene comes out empty. Direct mode works.
- [x] Put the desktop mode first in `Screen.resolutions`. Unity finds real
  display modes through SDL's video backend, and SDL reports none, so the
  engine falls back to its hardcoded table of safe resolutions - 35
  entries starting at 640x480. Megabonk stores index 0, so it asked for
  640x480 and drew a 4:3 image into a 16:9 window. Entry 0 is overwritten
  in place; layout is stride 16 of `{int width, int height, int refresh,
  int display}`.

### Thunks: host and guest must be rebuilt together

- [ ] Treat `libGL-host.so` and `libGL-guest.so` as one unit. The GLX
  bridge changed from a single implicit context to an explicit registry:
  `VEXA_BindGlxSurface(void)` became
  `VEXA_BindGlxSurface(uintptr_t context)` and
  `VEXA_CreateGlxContext` / `VEXA_DestroyGlxContext` were added. A new
  host with an old guest never populates the registry, so the bind returns
  early with no log line and `glXMakeCurrent` returns 0. Unity then prints
  "Unable to find a supported OpenGL Core profile" and exits - a black
  screen with no error that points at the real cause. Old+old works and
  new+new works; only mixed pairs fail.
- [ ] Ignore `fexldr_init_libGL missing symbol: VEXA_*`. It prints in
  working and broken runs alike. FEX resolves those through its generated
  interface, not `dlsym`, so it is not a fault indicator.
- [ ] Stage the host and guest thunk into the APK together. The first
  launch after an install re-stages the runtime from assets, and a
  half-updated pair silently breaks the game.

### Grass renders black (open)

- [ ] `Custom/GrassShaderOptimized` fails to link because Unity emits a
  vertex-stage SSBO and this Mali reports
  `GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS == 0`. Unity paints the object with
  a black fallback, which is why the ground is black while the bare path
  beside it renders correctly. Unity is told GL 4.3 by the spoof in
  `libGL_Host.cpp`, and `#version 430` is what it compiles to; the source
  is then rewritten to `#version 320 es` on the way to the driver, where
  the limit is the same.
- [ ] A vertex shader only reads such a block, so `buffer` -> `uniform` is
  the obvious rewrite. It gets past the link error and then fails with
  `S0018: Array size must be defined` - ES forbids an unsized array in a
  uniform block. Sizing it is not free: the grass instance struct is
  several mat4s, UBO space is around 64 KB, and the instance count is not
  known at shader compile time. Not solved.
- [ ] Do not reach for the runtime shader translator
  (`VEXA_ENABLE_RUNTIME_SHADER_TRANSLATOR`) to fix this. It needs shaderc
  and spirv-cross, neither is present, and it ends at `#version 320 es`
  anyway - SPIR-V is only an intermediate, the same driver limit applies
  afterwards. It would not have been a root fix.

## Later: Additional Categories
