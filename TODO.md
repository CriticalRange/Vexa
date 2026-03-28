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

## Later: Additional Categories

- [ ] Add new TODO categories here as runtime architecture evolves.
