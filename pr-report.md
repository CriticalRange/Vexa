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
