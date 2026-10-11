# PR Report — Logging fan-out, optional root diagnostics, and runtime fixes

Base: `9174fd7` (runtime: present guest Vulkan through an XCB surface)
FEX side: `4b1aa5fa1` (fex/android: thunk Vulkan WSI onto the Android surface)

## Summary

This run adds real observability to VEXA on Android: every layer of the app
now logs to logcat under a single `VEXA` tag, and an *optional* root-level
diagnostics layer (in-app toggle + host-side script) captures the signals a
normal app can never see — dmesg/SELinux audit, kernel stacks, debuggerd
backtraces, tombstones, cpuset/thermal state, and JIT memory dumps.

While validating it on the device (S23 FE, KernelSU) we also found and fixed
three real bugs: artifact dirs were never reset, the runtime worker was
locked out of the big CPU core, and a stale guest rootfs could silently
break XCB guests (`Cannot connect to XCB`).

## What changed

### 1. Logging: one stream, two sinks

- `logging/VexaLogger.kt` — entries now fan out to both the in-app
  `LogStore` **and** logcat (`adb logcat -s VEXA`), formatted as
  `[CATEGORY] message key=value` per `docs/DEBUGGING_STRATEGY.md`.
- `cpp/logging/native_log.cpp` — `VexaNativeLogRaw` now calls
  `__android_log_print` directly. FEX LogMan output reaches logcat from
  arbitrary native threads **without** attaching them to ART (the previous
  JNI path was disabled for exactly that risk). The JNI `NativeLogSink`
  path remains for normal native→Kotlin propagation.
- `logging/LogCategory.kt` — new `ROOT` category so root diagnostics are
  filterable in the debug panel.

Verified: `adb logcat -s VEXA` shows the full pipeline — ACTIVITY →
SURFACE → BOOT → RUNTIME → FEX LogMan → guest stdout/stderr.

### 2. Optional root diagnostics

Disabled by default; safe on unrooted devices.

- `runtime/RootLogCollector.kt` (new) — gated by `VEXA_ROOT_LOGS=1` in the
  launch env or the `files/root_logs_enabled` flag. Probes several `su`
  paths (5s timeout each), then per launch:
  - streams `dmesg -w` into `<artifacts>/root_dmesg.log`,
  - writes `root_snapshot_{start,destroy}.txt` (proc status/smaps, cpuset,
    oom, live syscall+kernel stacks, per-thread stacks, fd table,
    debuggerd backtrace, filtered dmesg, tombstones, thermal/cpufreq/PSI),
  - migrates the worker to the `top-app` cpuset (see bug #2).
  - No `su` → one `WARN [ROOT]` line and everything else runs unchanged.
- `RuntimeWorkerService.kt` — session lifecycle is wired to start/stop;
  each launch also logs `pid/cpuset/oomAdj` so the pre-remediation state is
  visible.
- `MainActivity.kt` — launcher "Root logging" checkbox writes the flag and
  shows live probe status (`Checking for su…` / `su: <path>` /
  `No root for this app — grant in KernelSU Manager (safe no-op)`).
  Toggle flips + flag writes verified on-device.

- `scripts/pull_root_logs.sh` (new, host side) — collects via `adb shell su`:
  device identity (bootloader/kernel, spoof-proof), per-process dumps,
  `debuggerd -b`, dmesg, tombstones + ANR files mentioning the package,
  VEXA logcat, app files/artifacts. Flags: `--watch N` (stream dmesg+logcat
  + periodic worker backtraces), `--perf N` (simpleperf stat + `record -g`
  → `perf.data`), `--dump-jit` (rwxp/exec-anon regions from
  `/proc/<pid>/mem` for offline disasm), `--enable-root/--disable-root`.
- `scripts/vexa_proc_dump.sh`, `scripts/vexa_sys_dump.sh` (new) — on-device
  helpers that keep `su -c` quoting sane; they dump everything `run-as`
  can't: kernel stacks, syscall args, schedstat, dmabuf fdinfo, mountinfo,
  auxv/environ, net/*, thermal zones, cpufreq/devfreq, PSI, cpuset groups,
  SELinux avc stats, dumpsys thermalservice/gfxinfo/meminfo, pstore.

Root-only extras confirmed working on-device: per-thread kernel stacks,
`avc: granted/denied` audit lines, tombstones, `debuggerd -b`, `/proc/pid/mem`
dumps (2×32 MB jit-cache regions pulled), simpleperf counters,
`dmesg -w`, `/data/anr` pull.

### 3. Bugs found by the new logging — and fixed

- **`resetArtifactsDir` never reset anything** (`RuntimeWorkerService.kt`).
  The guard compared the requested `files/artifacts/<guest>` with the base
  `files/artifacts` for *equality*, so every launch failed with
  `WARN Failed to reset artifacts directory` and old artifacts accumulated.
  Now a containment check (`inside files/artifacts`) and the **requested**
  per-guest dir is reset. Verified: `Artifacts directory reset for fresh run`.
- **Worker locked off the big core.** `:runtime` and `:runtime_worker` sat in
  `/foreground` cpuset (cpus 0-6) while the UI got `/top-app` (0-7) — FEX
  could not use the 2.8 GHz X-core. Two-part fix:
  `BIND_IMPORTANT` on the worker bind lifts its importance
  (`oom_score_adj` 740 → 100, verified), and since this ROM still doesn't
  move bound services out of `/foreground`, the root session additionally
  writes the pid to `/dev/cpuset/top-app/cgroup.procs`. Verified live:
  `worker cpuset=/top-app`.
- **Stale rootfs silently broke XCB guests** (`stage_runtime_assets.py`).
  `find_elf()` picks the *largest* `libxcb.so.1*` in the local guest
  rootfs — a stale rootfs shipped the real 162 KB Ubuntu socket client
  over the 12 KB VEXA shim → `vkcube: Cannot connect to XCB`. Added
  `FAKE_LIB_MARKERS`: any staged `libxcb.so.1`/`libX11.so.6` must contain
  the `"VEXA"` byte marker the shims embed, otherwise the build fails with
  rebuild instructions. Both directions tested.
- **`stage_rootfs.py` couldn't read Ubuntu noble debs** — `data.tar.zst`
  needs zstd, which `tarfile` only handles on Python ≥3.14. `extract_data`
  now streams `.zst` through the `zstandard` package.
- Removed dead placeholder log emit in worker `onCreate` (fired before
  `super.onCreate()`, could never be delivered).

### 4. Build/environment work (to make the pulled commits actually run)

- FEX: initialized `External/Vulkan-Headers` submodule (1.4.337 — required
  by the new WSI thunk), reconfigured `build-android-arm64-ninja` with
  `BUILD_THUNKS=ON`, built `libvulkan-{host,guest}.so`.
- Rebuilt `guest-tests/graphics/rootfs` (99 debs: vulkan-tools,
  mesa-utils-bin, xkbcommon, libdecor…) — `compile_fake_xcb` produced the
  shim `libxcb.so.1`.
- Note: `ro.product.*` on this device is spoofed to an ASUS/AI2501 identity;
  `ro.boot.bootloader` (`S711BXXSJGZI4`), kernel version, and tombstone
  fingerprint confirm a **Galaxy S23 FE** (r11s, Exynos 2200).

## Verification (S23 FE, KernelSU)

- `VEXA Hello`: stdout/stderr in logcat; root session wrote
  `root_dmesg.log` (329 KB) + start/destroy snapshots.
- `vkcube --wsi xcb`: `vkCreate*Surface -> 0` on the ANativeWindow,
  `Selected GPU 0: Samsung Xclipse 920` — cube renders at ~16% CPU,
  screenshot confirmed.
- `pull_root_logs.sh --perf 5 --dump-jit`: 3 process dumps + `system.txt`
  (thermal zones: BIG=71 °C, Thermal Status 3) + `perf.data` + filtered
  dmesg, all populated.
- Kernel-side audit: zero SELinux denials against VEXA; `avc: granted
  { execute }` recorded for the guest binary, loader, and libs. No
  segfaults/OOM/tombstones.

## Known non-issues

- `prctl failed errno 1` from FEX is an app-context EPERM, not SELinux —
  cosmetic.
- `fexldr_init_libvulkan missing symbol: vkCmdTraceRays*/vkCmdDrawMesh*`
  — the Samsung driver lacks those extensions; thunk lazy-proc path
  continues fine.

## Files

New: `runtime/RootLogCollector.kt`, `scripts/pull_root_logs.sh`,
`scripts/vexa_proc_dump.sh`, `scripts/vexa_sys_dump.sh`.
Modified: `VexaLogger.kt`, `LogCategory.kt`, `native_log.cpp`,
`RuntimeService.kt`, `RuntimeWorkerService.kt`, `MainActivity.kt`,
`stage_rootfs.py`, `stage_runtime_assets.py`, `docs/DEBUGGING_STRATEGY.md`
(new "Root-Level Diagnostics" section).

---

# Phase 2 — UI overhaul + physical-device bugfix pass

Driven end-to-end on the real S23 FE via ADB: install → tap → screencap →
inspect → fix → rebuild → reinstall. Every fix below was reproduced and
verified on the physical device.

## UI redesign

- New design system: `ui/theme/` (Color/Shape/Type/Theme tokens — `VexaBlack`
  base, cyan→violet accent gradient, `MonospaceTypography`) plus 14 shared
  components in `ui/` (`Brand`, `Buttons`, `Panels`, `Labels`, `Logs`,
  `Metadata`, `Pickers`, `Stats`, `Switches`, `Loaders`, `Hud`, `Effects`,
  `Previews`, `LogColors`). Launcher and debug panel now share one visual
  language; zero hardcoded hex colors remain in screens.
- Adaptive `>_` launcher icon (`ic_launcher_foreground.xml`, monochrome mask).
- `docs/UI_DESIGN.md` documents the token/component contract.
- Functional bugs in the old UI fixed along the way: `PickerCard` had no
  `onSelect` (dropdown could never actually select), and
  `formatLogTimestamp` rendered epoch ms as elapsed time
  (`29860809:10`) — now wall-clock `HH:mm:ss.SSS`.

## Bugs found during device runs — fixed

- **Loading overlay never dismissed** (`GameActivity.kt`).
  `showLoading` was initialized `true` and *never set false* — harmless
  when the old overlay was a transparent Box, but the new opaque overlay
  permanently covered the running guest (glmark2 scored 89 behind a
  spinner). Now dismisses on the first guest stdout/stderr line, on worker
  disconnect, or via a 12 s watchdog; `AnimatedVisibility` fades it out.
- **Orphaned `dmesg -w` processes** (`RootLogCollector.kt`).
  `stop()` called `Process.destroy()` which only killed the `su` wrapper —
  the `dmesg` child was reparented to init and kept appending (8 orphans,
  43 MB+ logs found on device). Worse, when the guest self-exited
  (EXIT_SELF → native `exit()`), `onDestroy`/`stop` never ran at all.
  Fixes:
  - the `su -c` command now runs a **watchdog shell**:
    `dmesg -w > file & echo $!; while kill -0 <workerPid>; do sleep 2; done;
    kill $!` — the stream dies with the worker regardless of exit path,
  - the real dmesg pid is captured from stdout and `stop()` kills it
    explicitly (no pkill self-match),
  - `stop("exit")` runs when `RuntimeBridge.startRuntime` returns, so
    EXIT_SELF runs get a `root_snapshot_exit.txt` too.
  Verified: `pgrep -x dmesg` → 0 after runs; exit snapshot written.
- **Debug panel header squeeze** (`GameDebugPanel.kt`). Title, stats and
  action buttons shared one crowded Row — "DEBUG CONSOLE" wrapped one
  letter per line and Close/Filters were crushed. Split into a title+actions
  row and a separate stats row (`logs / err / filtered` chips).
- **Debug panel scroll jank** (`LogStore.kt`, `GameDebugPanel.kt`,
  `Logs.kt`). Three compounding causes, all fixed:
  - `LogStore.append` copied the whole list per log line
    (`_entries.value + entry`) **on the main thread** — during thunk log
    bursts (~100+ lines/s) that was O(n²) work blocking frames. Now appends
    are O(1) into a buffer; a daemon flusher publishes a snapshot at most
    every 120 ms; entries are capped at 4000 (ring-buffer semantics).
  - `entries.filter` + `entries.count` ran two O(n) scans per emission —
    folded into a single `remember` pass.
  - New snapshots still interrupted in-flight scroll gestures mid-frame —
    the panel now **holds list updates while scrolling**
    (`snapshotFlow { isScrollInProgress }.first { !it }`), a standard
    log-viewer behavior; appends apply when the gesture ends without
    shifting the viewport (keyed items).
- **Debug chip unreachable / dead HUD** — same dismiss fix; the top
  debug pill was removed from the loading overlay per feedback.

## New UI features this pass

- **Scrollbar** in `LogList` (launcher + debug panel): estimated-height
  cyan thumb + track on the right edge, brightens while scrolling
  (`isScrollInProgress` → alpha 0.30→0.85), auto-hides when content fits,
  `contentPadding(end=8dp)` so rows never sit under the bar.
- **Fade transitions**: loading overlay fades out (450 ms) as HUD chips
  fade in (500 ms); debug panel `fadeIn(300)+scaleIn(0.97)` /
  `fadeOut(220)+scaleOut(0.97)`.
- `Modifier.animateItem()` on log rows — new entries slide in smoothly.

## On-device verification (S23 FE, KernelSU)

- `vkcube --wsi xcb` renders live on `Samsung Xclipse 920`; debug panel
  opens over the running guest, scrolls smoothly through 1100+ entries,
  `[ROOT]` entries visible in-panel (dmesg pid, cpuset remediation,
  snapshot writes).
- `glmark2-es2`: `glmark2 Score: 89`, `FPS: 90`, clean EXIT_SELF
  (importance=100 → `oom_score_adj` fix confirmed).
- `vexa-hello`: output logged, overlay dismissed, `root_snapshot_exit.txt`
  produced.
- Post-run `pgrep -x dmesg` → 0; worker `/proc/<pid>/cpuset` =
  `/top-app` during runs.
- Device identity confirmed again as S23 FE (r11s / Exynos 2200) despite
  spoofed `ro.product.*` ASUS props.

## Files (phase 2)

Modified: `GameActivity.kt`, `ui/GameLoadingOverlay.kt`,
`ui/GameDebugPanel.kt`, `ui/Logs.kt`, `logging/LogStore.kt`,
`runtime/RootLogCollector.kt`, `runtime/RuntimeWorkerService.kt`.
New: `ui/theme/*`, `ui/{Brand,Buttons,Panels,Labels,Logs,Metadata,Pickers,
Stats,Switches,Loaders,Hud,Effects,Previews,LogColors}.kt`,
`res/drawable*/ic_launcher*`, `docs/UI_DESIGN.md`.

---

# Phase 3 — PrismLauncher (Qt6 desktop GUI) on the FEX runtime

Goal of this pass: run a real x86_64 Qt6 desktop application — PrismLauncher —
inside the FEX guest, drawn on the app's own surface, with usable touch
input. There is no X server on Android; the guest binds the VEXA
libxcb/libX11 shims and Qt presents through `eglfs_x11` → ANativeWindow.

## Where we stopped

PrismLauncher is installed under `files/guests/prismlauncher` and **boots
into its real Qt6 GUI on the device screen** — main window, centered
dialogs, settings pages all render and take touch. The "Add Microsoft
account" button hands its OAuth URL to the phone's browser through the
openUrl bridge (below) and the Microsoft sign-in page is where the session
ended — login itself was reached but not completed.

Next steps when resumed: finish sign-in → the guest writes
`data/accounts.json` (the in-app watcher auto-foregrounds the game when it
changes), then instance creation and the Java runtime download — `java`
spawns through the new `FexChildMain` execve bridge, which is the next
thing to verify end-to-end.

## What changed

### 1. Fake X11/XCB layer grew up to Qt6 size (+~1000 lines)

Qt's xcb platform plugin exercises far more of the core protocol than
vkcube/glmark2 did:

- `guest-tests/x11-fake/libxcb_fake.c` (+714) — full core-protocol surface:
  GC/pixmap/cursor/colormap/property/window/keyboard cookie+reply stubs
  (new `libxcb_fake_qt.c` / `libxcb_fake_qt2.c` carry most of them).
  `xcb_wait_for_event` now **blocks like a socket read** instead of
  returning null — a null return means I/O failure and made Qt's xcb
  event-reader thread tear the whole connection down. The setup reply's
  screen size is read from `/tmp/vexa-window-size`, with physical mm
  sized for ~96 DPI so Qt keeps `devicePixelRatio=1` (scaling comes from
  `QT_SCALE_FACTOR` alone).
- `guest-tests/x11-fake/libX11_fake.c` (+97) — internal `_X*` entry points
  (`_XGetRequest`, `_XRead`, `_XReply`, `_XEatData`, async/error handlers…)
  that glvnd's `libGLX.so.0` imports as `DT_NEEDED`; under
  `QT_XCB_GL_INTEGRATION=xcb_egl` the GLX path never executes but the
  linker demands the symbols. `XCreateWindow` now logs geometry.
- `guest-tests/x11-fake/libX11xcb_fake.c` (new) — `XGetXCBConnection`
  returns the VEXA shim connection so Qt6's XOpenDisplay→xcb handoff stays
  inside the fake stack; `XSetEventQueueOwner` stub.
- `guest-tests/x11-fake/libdbus_fake.c` — grew from one variable to ~80
  stubs; enough for QtDBus to degrade cleanly to no-bus.

### 2. Two new LD_PRELOAD shims

- `guest-tests/notmpfile/libnotmpfile.so` (new) — `QTemporaryFileEngine`
  prefers `open(O_TMPFILE)` + `linkat("/proc/self/fd/N")`, but SELinux
  denies hardlinks inside app-private storage (EPERM) — this was
  PrismLauncher's `QtLockedFile` failure. Interposing every open/openat
  alias (incl. `__open64_nocancel` etc.) to return `EOPNOTSUPP` makes
  glibc/Qt fall back to named temp + rename. Also supplies the legacy
  fixed-function GL symbols (`glOrtho`, `glLoadMatrixf`) that
  `libQt6OpenGL` links unconditionally but the VEXA GL thunk can't export,
  and installs guest SIGABRT/SIGSEGV/SIGILL handlers that dump a DWARF
  backtrace to stderr before re-raising (a guest abort() reaches the host
  as a real signal through FEX's tgkill passthrough and used to kill
  `runtime_worker` silently).
- `guest-tests/tuio-fix/libvexa_tuiofix.so` (new) — two interpositions:
  - `QWindowSystemInterface::handleTouchEvent` — qtuiohandler delivers
    every touch to the *focused* window and computes position as
    `win->pos() + n*win->size()`; for non-fullscreen dialogs that hits the
    wrong window and Qt's modal redirection clamps out-of-bounds points
    onto edge widgets. The shim recovers the normalized coordinate, maps
    it onto the primary screen, and retargets the event to the smallest
    visible top-level window containing the point.
  - `QDesktopServices::openUrl` — the guest has no browser and Qt's
    xdg-open spawn uses a `clone()` form FEX doesn't support (it killed
    the whole guest on "Add Microsoft account"). The shim writes the URL
    to `$TMPDIR/vexa-open-url` for the Android host.

### 3. GameActivity: touch in, URLs out, background-proof

- **SurfaceView → TextureView** with a persistent `Surface`: TextureView
  keeps its `SurfaceTexture` across background trips, so the guest's EGL
  surface (bound once, never recreated) survives — no more dead guest
  after leaving/returning. `onSurfaceTextureDestroyed` returns false to
  keep it alive; released in `onDestroy` when actually finishing.
- **`TuioTouchSender`** — Qt can't reach `/dev/input` from the sandbox, so
  MotionEvents are translated to TUIO 2.0: OSC `#bundle` packets
  (`alive`/`set`/`fseq` on `/tuio/2Dcur`) over UDP loopback :3333, fed to
  Qt's `libqtuiotouchplugin` (a real host socket via FEX passthrough).
  Positions are normalized against the view, not the EGL surface
  (SurfaceFlinger may scale).
- **File-based guest↔host channel** (`startTuioCommandPoller`, 120 ms):
  - `<rootfs>/tmp/vexa-tuio-cmd` — `tap/down/move/up x y` commands so
    scripts can drive dialog-relative taps physical touches can't express.
  - `vexa-open-url` — read, deleted, fired as `ACTION_VIEW` intent. When
    the URL is a `redirect_uri=http://127.0.0.1` OAuth flow (Microsoft
    login), the poller arms a watcher on Prism's `accounts.json`
    (10 min deadline): on modification it brings the game back to front —
    `REORDER_TASKS`/`moveToFront`, `REORDER_TO_FRONT` intent, and a 1-px
    `TYPE_APPLICATION_OVERLAY` view to keep the activity foreground-eligible.
- **Runtime survives backgrounding**: `startService()` alongside bind in
  `onStart` (bind alone would destroy the service on unbind).
- Manifest: `REORDER_TASKS` + `SYSTEM_ALERT_WINDOW` permissions,
  `GameActivity` now `exported=true` for external launch/bring-to-front.

### 4. Guest execve bridge — `FexChildMain.kt` (new)

FEX emulates guest `execve()` by exec'ing `/proc/self/exe` — on Android
that's `app_process64`, not a FEXLoader. `GuestInstaller` now exports
`VEXA_APK`/`VEXA_FILES`/`VEXA_PROGRAM`, and the execve handler rewrites
argv to `app_process … runtime.FexChildMain <file> <args>` carrying the
guest env losslessly as `VEXA_ENV<i>` entries (raw `LD_PRELOAD` must never
reach a host exec — Bionic treats an unloadable preload as fatal,
`CANNOT LINK EXECUTABLE`). `FexChildMain.main()` rebuilds the
`LaunchRequest`, loads `libvexa.so` from the APK's lib dir, and re-runs
the child under FEX — this is the path PrismLauncher takes to spawn
`java`. New `busybox-filetest` catalog entry isolates guest file/exec
behavior for exactly this.

### 5. PrismLauncher guest program → real GUI config

- `shared/bin/prismlauncher -d <data dir>` with `QT_QPA_PLATFORM=eglfs` +
  `QT_QPA_EGLFS_INTEGRATION=eglfs_x11`, `QT_PLUGIN_PATH` at the bundled Qt
  6.10.2, `QT_QPA_GENERIC_PLUGINS=tuiotouch`,
  `QT_TUIOTOUCH_DELIVER_WITHOUT_FOCUS=1`, `TMPDIR`/`XDG_RUNTIME_DIR` inside
  the guest dir, `LD_PRELOAD=libnotmpfile.so:libvexa_tuiofix.so`.
- `LD_*` paths are deliberately **guest-only** (`/usr/lib/…`): if the env
  leaks into a host-side exec the linker prints "not found" and moves on;
  a host-resolvable x86_64 path is what produced the `CANNOT LINK
  EXECUTABLE` seen on Prism's java check.

### 6. Rootfs/thunk restage + OpenAL removal

Rootfs assets rebuilt (`STAMP` → `4d0ddf1c2f572730`): refreshed fake
`libX11/libXext/libxcb/libdbus-1/libxkbcommon-x11`, and the OpenAL stack
(`libasound.so.2`, `libopenal{,-guest,-host}.so`) is gone — audio for this
form factor is being rethought separately. `crash_signals.cpp` now walks
the guest frame-pointer chain (`guest_bt_ret`) and dumps 160 raw stack
words (`guest_stk`) via `process_vm_readv` on crash.

### 7. Parallel track: native chroot comparison (scripts/)

While debugging Qt-on-FEX we also built the "does it work *natively*?"
control: `install_prism_portable.sh`, `install_store_and_brave.sh`,
`install_user_store.sh` install PrismLauncher (aarch64 build), Brave and
Synaptic into the Andronix Ubuntu chroot; `start-root-linux.sh` /
`stop-root-linux.sh` / `app.sh` bring up PulseAudio + Termux-X11 + the
chroot as a zero-desktop app launcher; `termux_bashrc` wires the Termux
side. Useful as a reference for what the GUI is *supposed* to do.

## Files (phase 3)

New: `runtime/FexChildMain.kt`, `guest-tests/notmpfile/` (`.c` + `.so`),
`guest-tests/tuio-fix/` (`.cpp` + `.so`), `guest-tests/x11-fake/
{libX11xcb_fake.c,libxcb_fake_qt.c,libxcb_fake_qt2.c}`,
`scripts/{install_prism_portable,install_store_and_brave,
install_user_store,start-root-linux,stop-root-linux,app}.sh`,
`scripts/termux_bashrc`.
Modified: `AndroidManifest.xml`, `GameActivity.kt`, `GuestInstaller.kt`,
`GuestProgram.kt`, `cpp/logging/crash_signals.cpp`,
`guest-tests/x11-fake/{libX11_fake,libxcb_fake,libdbus_fake}.c`, bundled
rootfs/thunk assets (OpenAL removed).
