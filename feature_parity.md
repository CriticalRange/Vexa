# FEX Interpreter Feature Parity

Scope: parity checklist between VEXA runtime wrapper and upstream `FEXInterpreter` startup flow.

Reference baseline:

- `~/FEX/Source/Tools/FEXInterpreter/FEXInterpreter.cpp`
- `app/src/main/cpp/runtime/launch.cpp`
- `app/src/main/cpp/runtime/init/*.cpp`

## 1) Bootstrap and Config

- [x] Install LogMan message/assert handlers before runtime initialization.
- [x] Load config and reload meta layer (`LoadConfig` + `ReloadMetaLayer`).
- [x] Set core config keys (`ROOTFS`, thunk host/guest, app filename/config name, output log, is64
  mode).
- [x] Pass real environment and portability info into config load (partially completed).
  full implementation: plumb `envp` into native start path and call
  `FEX::ReadPortabilityInformation()` before `FEX::Config::LoadConfig(...)`.
- [x] Canonical app naming parity (`realpath` fallback / anonymous FD mode handling) (partially
  completed).
  full implementation: replicate interpreter logic for `CONFIG_APP_FILENAME` and
  `CONFIG_APP_CONFIG_NAME` including `<Anonymous>` when launch is FD-backed.
- [x] Initialize FEXServer client (`FEXServerClient::SetupClient`) before rootfs-sensitive
  operations.
- [ ] Initialize upstream logging backend (`FEX::Logging::Init`) and server log path routing.
- [ ] Apply host environment entries from config (`HostEnvironment` -> `putenv` loop).
- [ ] Kernel-version compatibility check and startup policy hooks (`STALLPROCESS` / `STARTUPSLEEP`).
- [ ] GCS/shadow stack safety check (`FEX::Kernel::GCS::CheckForGCS`).

## 2) Core Runtime Init

- [x] Fetch host features and create context.
- [x] Create and bind signal delegator, thunk handler, and syscall handler to context.
- [x] Initialize core (`InitCore`).
- [x] Initialize Linux emulation thread handlers (`SetupThreadHandlers`) (partially completed).
  full implementation: call `FEX::LinuxEmulation::Threads::SetupThreadHandlers()` early, store
  returned tracker, and shut it down during cleanup.
- [x] Initialize allocator parity (`FEX::Allocator::Init`) (partially completed).
  full implementation: call allocator init using loader bitness and handle allocator shutdown on
  teardown.
- [x] Initialize kernel compatibility modes (`FEX::Kernel::Init`) (partially completed).
  full implementation: call `FEX::Kernel::Init(loader.Is64BitMode(), ctx.get())` after context
  creation.
- [ ] Initialize telemetry and profiler (`Telemetry::Initialize`, `Profiler::Init`).
- [ ] 32-bit syscall path parity (partially completed).
  full implementation: select x64/x32 syscall handler based on loader bitness and supply x32
  allocator path.

## 3) FD Ownership and Conflict Handling

- [x] RootFS FD tracking exists in upstream FileManager and is active in current build.
- [ ] Track additional FEX-owned FDs (logging/server FDs) in FileManager (partially completed).
  full implementation: after handler/log init, call `SyscallHandler->FM.TrackFEXFD(...)` for
  output/server fds as interpreter does.
- [ ] `/proc/self/fd` conflict hardening under Android sandbox restrictions (partially completed).
  full implementation: verify `ProcFD`/inode tracking is valid and ensure protected FD hiding
  remains active; if unavailable, add explicit mitigation for protected FD close attempts.
- [ ] Close/close_range protected FD behavior hardening for RootFSFD and other internal FDs.

## 4) ELF Load and Execute

- [x] Preflight path checks for working dir/rootfs/thunk dirs/executable/artifact dir.
- [x] ELF type detection and loader creation.
- [x] `SetCodeLoader`, VDSO load, thunk definition append, VDSO symbols setup.
- [x] Map memory and set default program break.
- [x] Set initial RIP/RSP and execute thread.
- [x] Close loader FDs after mapping.
- [ ] Seccomp fd restore path (`DeserializeSeccompFD`) support.
- [ ] Code cache path parity (`SetCodeMapWriter` + `PopulateCodeCache`) (partially completed).
  full implementation: wire both config-gated branches as in interpreter flow.
- [ ] `AT_EXECFD` / `FEX_EXECVEFD` launch mode parity.
- [ ] Shebang/interpreter rewrite parity (`InterpreterHandler`) for script entrypoints.
- [ ] Full argv/parsed-args/env forwarding parity (partially completed).
  full implementation: pass full argument vectors and envp instead of only `argv[0]`.

## 5) Cleanup and Shutdown

- [x] Uninstall TLS state and destroy parent thread.
- [x] Reset context-attached handlers and release owned runtime objects.
- [x] Shutdown config and uninstall log handlers.
- [ ] Stop thread manager (`TM.Stop`) before teardown.
- [ ] Free loader sections (`Loader.FreeSections`) after execution path ends.
- [ ] Shutdown allocator and Linux emulation thread handler tracker.

## 6) Optional/Debug Parity

- [ ] GDB server integration (`GdbServer` config path).
- [ ] Startup sleep/stall diagnostics parity.

## Current Priority (for `Close closing FEX FD ...`)

- [ ] Ensure all FEX-owned FDs are tracked and protected from guest close paths.
- [ ] Add explicit guard/mitigation for guest `close`/`close_range` hitting protected internal FDs.
- [ ] Verify proc-based hidden-FD path remains active on Android runtime worker process.
