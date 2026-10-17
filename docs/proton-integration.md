# Proton process integration

## Scope and validation status

VEXA routes supported x86 Linux ELF executables through FEX and supported Windows
PE executables through a separate Proton session. Selection uses executable
headers rather than the filename extension. Linux Steam client `execve` calls
inside FEX use that dispatch path. An Android-side Steam launch adapter exists,
but no Android Steam callback is currently wired to call it.

This implements launch orchestration and an experimental real-X11 transport,
not complete Windows game support. On the connected Android device, a
GE-Proton10-34 `notepad.exe` window rendered through FEX and genuine headless
X11 after the latest FEX fixes. No graphical game or FPS result has been
verified.

## Process model

```text
Android runtime worker / Steam launch request
  -> inspect executable header
     -> x86 ELF: existing FEX runtime
     -> x86 PE: native session supervisor
          -> standalone vexaloader process
             -> FEX executes guest x86-64 Python
                -> VEXA Proton session script
                   -> upstream Proton entrypoint
                      -> Wine, wineserver, game and helper processes
```

The entrypoint is the upstream Python program, not a reimplementation of Proton
in Kotlin or C++. The installed bundle controls prefix setup, migrations, Wine
configuration and its DLL components. Guest Python keeps subsequent Linux
process execution inside FEX; Android's kernel is not expected to execute an
x86 Wine ELF through system-wide binfmt registration.

The standalone loader and FEX reexec integration are therefore essential for
subprocesses. Interpreter scripts, ELF architecture, descriptor-backed execution
and inherited environment must survive those transitions. A successful initial
Python launch is insufficient if a later Wine or wineserver exec fails.

Current support is pathname-based exec. FD-backed PE exec explicitly returns
`ENOTSUP`; descriptor-backed Linux re-exec and dirfd-relative execveat are not
claimed by the current trampoline ABI. These need a separate descriptor/argv
protocol extension before accepting an application that depends on them.

The supervisor isolates a process group and tags the session for cleanup.
Persistent compatdata is separate from transient process lifetime. The session
wrapper must account for Wine helpers and wineserver lifetime, rather than
assuming the first launcher process represents every game process.

## Deployment prerequisites

1. Install a **complete, pinned x86-64 Linux Proton distribution** using the
   modern `files/` layout. Keep the entire distribution, its modules, libraries,
   default prefix resources and version metadata together. A copied `proton`
   script or a standalone Wine executable is not sufficient. Legacy `dist/`
   packages and a separate ARM64EC Wine/FEX-DLL installation are different
   deployment models and are not this launch contract.
2. Supply a compatible guest glibc rootfs with **x86-64 Python 3 and its matching
   full standard library**, including native extension modules, plus the Linux
   userland and shared libraries needed by the selected Proton release. Python
   must run under FEX. Android Python, Android shell utilities and bionic
   libraries are not substitutes for these guest dependencies. Static inventory
   checks cannot prove dynamic-linker or runtime compatibility.
3. Supply a genuine **Linux Steam client installation**. Its
   `linux64/steamclient.so` is required by the launch preflight; retain the
   corresponding client installation and compatibility resources rather than
   copying only that file. VEXA's Android `libsteamclient.so`, JavaSteam login
   state and Linux x86 Steam libraries have different roles. Account login alone
   does not implement the Linux Steam IPC services required by every title.
4. Install the bundled VEXA guest session helper at
   `/opt/vexa/proton_session.py` in the deployed rootfs, or configure its path.
5. Provide writable persistent storage with Linux filename and symlink semantics
   for compatdata. Keep prefixes out of read-only runtime assets and document
   any per-game prefix migration before changing Proton versions.
6. Configure an explicit display backend as described below. The experimental
   real-X11 Vulkan bridge does not establish broad Wine or game compatibility.

For a trusted, already unpacked bundle, stage a new immutable version directory:

```sh
python tools/stage_proton.py --source /downloads/Proton \
  --destination /staging/proton-10 \
  --version proton-10 --rootfs /staging/proton-rootfs
```

`--source`, `--destination` and `--version` are required; `--rootfs` adds static
guest Python checks. The destination must be new and its final name must match
the version identifier. Use a filesystem that supports symlinks. The tool does
not download Proton, verify publisher signatures, or execute guest programs.
Its SHA256 manifest inventories staged content; it is not an authenticity or
game-compatibility certificate. A staging failure can leave a partial destination
for inspection. Packaging a complete rootfs and Proton bundle can substantially
increase artifact size; do not silently replace a user's existing runtime.

Deploy the staged distribution with a symlink/mode-preserving Linux archive
into the application's private directory. Do not put a full Proton distribution
through the current APK asset-tree copier: that copier does not preserve Linux
symlinks or executable modes. No Proton or Python binaries are bundled by this
change. The small VEXA session helper is installed independently by `ensureRuntime`.

Apply the checked-in external FEX changes, then rebuild both libraries:

```sh
python tools/apply_fex_compat.py --fex-root ../FEX
```

This adds PE-routing and standalone-exit hooks, preserves routing controls
across child environments, and transfers guest `LD_PRELOAD`/`LD_LIBRARY_PATH`
past the Android linker without applying them to native ARM64 binaries. It
also includes Android-specific filesystem, process-ID, memory-protection and
seccomp fallbacks needed by the observed Proton startup path. Rebuild
LinuxEmulation and VEXA together. The optional `--vulkan-x11` patch enables
the experimental X11 Vulkan WSI path; check the exact patch set before a build:

```sh
python tools/apply_fex_compat.py --fex-root ../FEX --check --vulkan-x11
```

## Configuration

The Android configuration file is `files/compatibility/proton.json`, relative to
the application's private files directory. Example device-side paths:

```json
{
  "protonPath": "/data/user/0/com.critical.vexaemulator/files/proton/proton-10",
  "rootfs": "/data/user/0/com.critical.vexaemulator/files/proton-rootfs",
  "steamClientPath": "/data/user/0/com.critical.vexaemulator/files/steam-linux",
  "dataRoot": "/data/user/0/com.critical.vexaemulator/files/compatdata",
  "python": "/usr/bin/python3",
  "displayBackend": "external-x11",
  "display": ":1"
}
```

These are examples, not instructions to assume the package's private path is
identical on every Android installation. `protonPath`, `steamClientPath` and
`dataRoot` describe device host paths; `python` is a guest path. File presence
must be checked after extraction onto the device.

The corresponding launch environment contract is:

| Variable | Meaning |
| --- | --- |
| `VEXA_PROTON_PATH` | Absolute device host path to the full Proton directory. |
| `VEXA_STEAM_CLIENT_PATH` | Absolute device host path to the Linux Steam installation. |
| `VEXA_COMPAT_DATA_ROOT` | Persistent host parent for per-game prefixes; defaults to `compatdata` beside the rootfs directory. |
| `VEXA_PROTON_PYTHON` | Guest x86-64 Python ELF; default `/usr/bin/python3`. |
| `VEXA_PROTON_ROOTFS` | Optional separate pinned guest rootfs; defaults to the launch rootfs. Keeps real Wine/X11 dependencies separate from native-game fake libraries. |
| `VEXA_PROTON_SESSION_SCRIPT` | Guest helper; default `/opt/vexa/proton_session.py`. |
| `VEXA_PROTON_DISPLAY_BACKEND` | Explicit `external-x11` or `headless`. |
| `DISPLAY` | Required for `external-x11`; must reach the actual configured server. |
| `SteamAppId` | Required nonzero numeric ID; use a stable private ID for non-Steam games. |

Each game uses `<dataRoot>/<SteamAppId>` as `STEAM_COMPAT_DATA_PATH`; Proton's
Windows prefix is its `pfx` subdirectory. The dispatcher sets the Steam client
and game installation paths and defaults `SteamGameId` to `SteamAppId` when
absent. Keep the same stable ID to retain that game's prefix. Do not point
`STEAM_COMPAT_DATA_PATH` directly at `pfx` or share one default prefix among games.

Proton controls its Wine DLL and library configuration. Do not insert Android
host `.so` directories into guest `LD_LIBRARY_PATH` or manually force a global
`WINEDLLPATH`. Version-specific overrides should be explicit game settings.
`PROTON_LOG=1` enables upstream logging; VEXA directs Proton logs to the launch
artifact directory.

## Graphics boundary

PE launch fails explicitly until a display backend is configured:

- `external-x11` requires `DISPLAY`, a genuine accessible X server, compatible
  guest X11 client libraries and a working Vulkan/GL presentation transport.
  VEXA can start the pinned Termux:X11 server from the headless activity. The
  experimental WSI shares Android hardware buffers with the server through
  DRI3/Present; a Wine Notepad window has rendered, but no game frame has.
  The guest rootfs must provide working Linux graphics and audio libraries.
- `headless` permits non-GUI process/prefix probes. It does not make graphical
  games render into VEXA's Android Surface.

The existing fake X11 libraries remain suitable only for their native-game
path. Wine uses real shared X server state, so compatibility sessions route to
the genuine server. Native Linux ELF games retain their existing direct
Android Vulkan/ANativeWindow path and do not pay for the Proton X11 path.

The Android `ANativeWindow*` stored in the runtime worker is process-local.
After standalone exec, a numeric pointer or inherited environment value cannot
restore it. Native Surface Parcel helpers require API 34 and real Binder object
transport. Raw Parcel bytes do not transfer those object references, and ordinary
apps cannot register arbitrary global native Binder services.

The current bridge imports `AHardwareBuffer` images into Vulkan and passes them
to the X server through DRI3/Present. It does not copy every frame through CPU
memory. Headless PNG capture is an optional diagnostic mode, disabled by
default. The WSI is still limited to a narrow set of surface and swapchain
features. Basic KHR v2 acquire, capabilities and format queries map to the
existing surface behavior; unsupported extension chains fail explicitly.
Presentation waits for the relevant Vulkan release
submission to complete before handing its buffer to X11; it no longer waits
for the entire queue to become idle. This still blocks the presenting thread
per frame. Explicit fence transfer to X11, pipelining and actual frame timing
remain follow-up work.

Pin DXVK/VKD3D to the supported driver capabilities of the chosen deployment.
Do not automatically select the newest component based only on Vulkan being
present: current DXVK 3.x advertises Vulkan 1.4 requirements and no first-party
Android/mobile driver support. Native Linux Vulkan rendering in VEXA does not
validate Wine's Vulkan WSI or Windows game feature requirements.

## Device acceptance still required

The headless Wine GUI, normal exit-code propagation and Stop cleanup probes
now pass. Next check direct and Steam-originated ELF/PE routing, arguments
with spaces, input, audio, and representative games. Descriptor-backed PE
exec remains unsupported. Record actual frame timing before making performance
claims.

## Checks and observations through 2026-10-16

- Rebuilt patched FEX `LinuxEmulation` for Android ARM64 successfully.
- Built and linked VEXA `vexa` and `vexaloader` targets successfully.
- Kotlin `compileDebugKotlin` passed with the new Steam request adapter/settings.
- Native strict syntax checks and Python byte-compilation passed. Classifier
  fixtures were syntax-checked, not executed on this host.
- Verified the linked library exports the PE, standalone-loader and loader-main
  entrypoints; verified the FEX patch is applied and whitespace checks pass.

- On the connected Android device, real headless X11 exposed an X.Org window;
  a FEX child process returned successfully. GE-Proton10-34, a Linux Steam
  client and a Debian x86-64 rootfs were staged for probes.
- A Proton prefix was created. A first wineserver attempt died under Android
  seccomp on host syscall 449 (`futex_waitv`). With the Android fallback patch,
  wineserver reported server-side synchronization but next failed to load
  `l_intl.nls`. The file existed in the staged Proton distribution.
- Subsequent FEX PID, mmap and seccomp patches check as applied, including the
  optional X11 Vulkan patch. Remaining Bionic-cached PID calls in the startup
  and thread path were changed to the raw-safe FEX helper. Steam SDK links now
  fail preflight explicitly if they cannot be created.
- Android launch routing now reads executable headers: catalog PE requests
  enter the real-X11 Proton activity; ELF requests retain the direct Android
  Surface path. A Steam-origin request can use the new adapter, but no Android
  callback invokes it yet. Guest Linux Steam client `execve` is handled by FEX.
  Headless PE launches select real X11 automatically.
  The focused classifier tests and combined debug APK/unit-test build passed.
- On Xiaomi 2407FPN8EG, the 2026-10-16 debug APK ran Proton
  `files/lib/wine/x86_64-windows/notepad.exe` as App ID `90000006` via
  `HeadlessLaunchActivity`, FEX, GE-Proton10-34 and the real X server. The
  headless capture showed the Notepad menu, blank editor and status bar.
  Wine loaded `wineboot.exe`, `user32.dll` and `gdi32.dll`; the previous
  `l_intl.nls` error and syscall-449 SIGSYS did not recur in this attempt.
  This probe used `PROTON_USE_WINED3D=1`; it does not validate DXVK or Vulkan
  game presentation.
  Issuing the activity's Stop command removed the standalone loader and
  X-server processes. `proton-session.json` remained at `starting` because
  Stop intentionally terminates the session rather than recording a normal
  exit. This is a GUI smoke test, not Windows game compatibility proof.
- Reusing the `90000006` prefix, `cmd.exe /c exit 0` ended normally after
  71.783 seconds. `proton-session.json` recorded `proton_exit=0`,
  `wineserver_exit=0` and `exit_code=0`; no standalone loader processes
  remained. This validates the normal session completion path on this device.

## Upstream references

- [Valve Proton entrypoint and environment handling](https://github.com/ValveSoftware/Proton/blob/proton_10.0/proton)
- [Valve Proton distribution and runtime options](https://github.com/ValveSoftware/Proton)
- [FEX Proton notes](https://wiki.fex-emu.com/index.php/Proton)
- [FEX exec and execveat handling](https://github.com/FEX-Emu/FEX/blob/main/Source/Tools/LinuxEmulation/LinuxSyscalls/Syscalls.cpp)
- [FEX ARM64EC unixlib architecture](https://fex-emu.com/FEX-2607/)
- [Wine X11 driver initialization](https://github.com/wine-mirror/wine/blob/master/dlls/winex11.drv/x11drv_main.c)
- [Wine X11 Vulkan WSI](https://github.com/wine-mirror/wine/blob/master/dlls/winex11.drv/vulkan.c)
- [Android native window APIs](https://developer.android.com/ndk/reference/group/a-native-window)
- [Android hardware-buffer transport](https://developer.android.com/ndk/reference/group/a-hardware-buffer)
- [Android Binder reference transport](https://developer.android.com/reference/android/os/IBinder)
- [AOSP application service registration restrictions](https://android.googlesource.com/platform/system/sepolicy/+/refs/heads/android15-tests-release/private/app_neverallows.te)
- [DXVK driver requirements](https://github.com/doitsujin/dxvk/wiki/Driver-support)
