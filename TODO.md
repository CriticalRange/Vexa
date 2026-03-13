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
- [ ] Add a runtime snapshot API (read-only state/progress object) that exposes preflight/init/execute
  stage completion flags and last failure `Code/Phase/Reason`, so Kotlin/UI and dependent subsystems
  can gate behavior without parsing logs.
- [ ] Design and implement supervisor/worker split: keep `RuntimeService` as supervisor in `:runtime`
  and move native/FEX execution to a dedicated worker process (for example `:runtime_worker`), with
  Binder/Messenger forwarding and worker-death recovery.
- [ ] Add cross-process surface transport for worker mode: define explicit IPC for `Surface` handoff
  (Parcelable/Binder path), ownership lifecycle, and teardown semantics so worker-render path can
  consume the UI-created surface safely.
- [ ] Add latency budget and transport plan for high-frequency input in worker mode
  (`UI -> supervisor -> worker`), with criteria for when to keep Binder and when to switch to
  shared memory or socket-based fast path.
- [ ] Add worker-process file/FD permission validation for game/rootfs/thunk/artifact paths and any
  URI-backed inputs: ensure grants are applied before worker start and log explicit permission-denied
  diagnostics with failing path.
- [ ] Track and patch FEX `/proc/pid/cmdline` remap behavior on Android app UIDs:
  `prctl(PR_SET_MM, PR_SET_MM_MAP, ...)` fails without privileged caps (expected, non-fatal).
  Action: downgrade to warn-once or skip on Android, and keep launch flow unaffected.
- [ ] Add allocator parity follow-up for 32-bit mode: after `SetupHooks()` +
  `CreatePassthroughAllocator()`, mirror interpreter behavior that drains pre-reserved host allocator
  space (the malloc loop) before guest execution.

## Later: Additional Categories

- [ ] Add new TODO categories here as runtime architecture evolves.
