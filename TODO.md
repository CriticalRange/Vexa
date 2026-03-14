## Runtime TODO: FEX Bring-up Sequence

- [x] First priority: Compile VEXA runtime wrapper directly against FEX headers/libs
  (`target_include_directories` + `target_link_libraries`) and avoid string-based `dlsym` calls
  for mangled C++ symbols.
- [ ] Integrate `FEXCore::Config::Initialize()` in runtime startup and log success/failure as
  `FEX` category.
- [ ] Integrate `FEXCore::Config::Load()` after config layering, with explicit error reporting if
  config paths are invalid.
- [ ] Integrate `FEX::FetchHostFeatures()` and log key host feature flags used for runtime decisions
  (for example AVX and cache/atomic related flags).
- [ ] Create FEX context with
  `FEXCore::Context::Context::CreateNewContext(HostFeatures)` and fail fast if context creation
  returns null.
- [ ] Wire and set signal delegator (`CTX->SetSignalDelegator(...)`) and verify delegated signal
  path logging before guest execution.
- [ ] Wire and set syscall handler (`CTX->SetSyscallHandler(...)`) with structured logs for handler
  creation and attachment.
- [ ] Call `CTX->InitCore()` and treat non-success as a hard startup failure with dedicated failure
  code and diagnostics.
- [ ] Add initial thread lifecycle bring-up (`CreateThread` + `ExecuteThread`) behind preflight
  success, with guarded logging around enter/exit and crash boundaries.
- [ ] Add a runtime snapshot API (read-only state/progress object) that exposes
  preflight/init/execute
  stage completion flags and last failure `Code/Phase/Reason`, so Kotlin/UI and dependent subsystems
  can gate behavior without parsing logs.
- [ ] Design and implement supervisor/worker split: keep `RuntimeService` as supervisor in
  `:runtime`
  and move native/FEX execution to a dedicated worker process (for example `:runtime_worker`), with
  Binder/Messenger forwarding and worker-death recovery.
- [ ] Add cross-process surface transport for worker mode: define explicit IPC for `Surface` handoff
  (Parcelable/Binder path), ownership lifecycle, and teardown semantics so worker-render path can
  consume the UI-created surface safely.
- [ ] Add latency budget and transport plan for high-frequency input in worker mode
  (`UI -> supervisor -> worker`), with criteria for when to keep Binder and when to switch to
  shared memory or socket-based fast path.
- [ ] Add worker-process file/FD permission validation for game/rootfs/thunk/artifact paths and any
  URI-backed inputs: ensure grants are applied before worker start and log explicit
  permission-denied
  diagnostics with failing path.
- [ ] Track and patch FEX `/proc/pid/cmdline` remap behavior on Android app UIDs:
  `prctl(PR_SET_MM, PR_SET_MM_MAP, ...)` fails without privileged caps (expected, non-fatal).
  Action: downgrade to warn-once or skip on Android, and keep launch flow unaffected.
- [ ] Add allocator parity follow-up for 32-bit mode: after `SetupHooks()` +
  `CreatePassthroughAllocator()`, mirror interpreter behavior that drains pre-reserved host
  allocator
  space (the malloc loop) before guest execution.

## SDL3 Thunk TODO (FEX-Style, Android)

- [ ] Define scope/version: choose target SDL3
  SONAME(s) and exact API surface to thunk first
  (minimal boot set, then expand).
- [ ] Include `SDL3_image` in first-pass scope
  planning: Hytale references both `SDL3` and
  `SDL3_image`, so startup path must cover both
  libraries (thunk or compatible guest-lib strategy).
- [ ] Create upstream thunk library skeleton:
  `ThunkLibs/libSDL3/` with `libSDL3_interface.cpp`,
  `libSDL3_Guest.cpp`, `libSDL3_Host.cpp`.
- [ ] Create companion plan for `libSDL3_image`
  (`ThunkLibs/libSDL3_image/` or explicit non-thunk
  strategy) and define dependency/link order relative
  to `libSDL3`.
- [ ] Implement generator config (`fex_gen_config`,
  `fex_gen_type`, `fex_gen_param`) for SDL3 types/
  functions, including callback strategy and pointer/
  opaque annotations.
- [ ] Add build wiring in FEX CMake (`ThunkLibs/
  GuestLibs/CMakeLists.txt` and `ThunkLibs/HostLibs/
  CMakeLists.txt`) for `libSDL3` guest/host thunk
  targets.
- [ ] Add thunk DB mapping in `Data/ThunksDB.json`
  for SDL3 overlays (`Library`, `Overlay`, optional
  `Depends`) so guest SDL3 resolves to `libSDL3-
  guest.so`.
- [ ] Add/verify app config enablement (`ThunksDB`
  key for SDL3) so runtime actually turns SDL3
  thunking on.
- [ ] Ensure Android runtime paths are correct:
  `THUNKHOSTLIBS` and `THUNKGUESTLIBS` contain SDL3
  thunk artifacts and are preflight-validated.
- [ ] Resolve host SDL3 load path on Android
  (`dlopen` target used by generated host loader) and
  verify dependencies are present.
- [ ] Extend deploy scripts to push SDL3 host/guest
  thunk artifacts and verify on-device presence before
  launch.
- [ ] Add structured logs for thunk load lifecycle
  (`fex:loadlib`, host export init, symbol
  registration, overlay hit/miss).
- [ ] Validate callback round-trip
  (guest->host->guest) with canary SDL3 callback
  paths.
- [ ] Run incremental test matrix: startup, window
  init, input, audio, teardown, relaunch stability.
- [ ] Android audio backend plan: keep desktop
  `libasound` thunks disabled in Android builds and
  use `openal-soft` as primary audio replacement;
  keep SDL3 Android audio backend (`AAudio` /
  `OpenSLES`) as fallback path when title uses SDL
  audio directly.
- [ ] Add explicit OpenAL thunk track:
  define `openal` SONAME mapping, thunk surface, and
  host load behavior for Android (`openal-soft`).
- [ ] Define rendering ownership contract before
  broad thunk expansion:
  `SDL3` video path vs `libGL/libEGL` redirect path,
  and expected handoff points between them.
- [ ] Document known Android-specific deviations and
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

- [ ] Preserve `libGL` thunk ABI surface first: current
  `ThunkLibs/libGL/libGL_interface.cpp` is heavily
  `glX*` and X11-oriented, so replacement must keep
  expected entrypoints/signatures even if internals
  redirect.
- [ ] Keep `glXGetProcAddress*` compatibility behavior:
  `libEGL` guest thunk currently relies on
  `glXGetProcAddress` path (`ThunkLibs/libEGL/
  libEGL_Guest.cpp`), so redirect layer must still
  provide this contract.
- [ ] Preserve guest `libGL` init side effects:
  `libGL_Guest.cpp` wires callback hooks and implicit
  X11 dependency assumptions (`XSync`,
  `XGetVisualInfo`, `XDisplayString`, `libX11` pull-in).
- [ ] Keep SONAME/overlay identity unchanged:
  `Data/ThunksDB.json` overlay mapping for `libGL.so*`
  -> `libGL-guest.so` must remain stable while internals
  are replaced.
- [ ] Keep guest link graph valid:
  `ThunkLibs/GuestLibs/CMakeLists.txt` links
  `EGL-guest` to `GL-guest` and injects placeholder
  `libX11.so.6`; update only after confirming no symbol
  breakage.
- [ ] Gate desktop OpenGL host thunk on Android:
  current host build path requires desktop OpenGL/GLX
  (`find_package(OpenGL REQUIRED)`), so Android build
  must skip desktop `libGL` host thunk until redirect
  host implementation is ready.
- [ ] Implement Android host redirect layer in phases:
  phase 1 = compatibility shim + stubs, phase 2 = map
  viable calls to `EGL/GLES`, phase 3 = remove stubs
  only after runtime validation.

## Later: Additional Categories

- [ ] Add new TODO categories here as runtime architecture evolves.
