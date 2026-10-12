# VEXA + Unity Linux Player — Session Handoff

**Date:** 2026-10-12
**Target:** make Unity games (Megabonk, Vampire Survivors) render on the VEXA Android/Linux runner
**Current symptom:** game boots, Unity creates a real GL 4.3 device, runs game C#, but the
display stays black and Unity renders only 2 fullscreen blits per frame into a **256x256**
render target while the window/surface is **2712x1220**.

This document is self-contained. Everything below was verified on-device during this session.

---

## 1. Environment and how to drive it

| Item | Value |
|---|---|
| vexa repo | `C:\Users\zeytu\Documents\vexa` |
| FEX repo | `C:\Users\zeytu\Documents\FEX` |
| Device package | `com.critical.vexaemulator` |
| Launch | `adb shell am start -n com.critical.vexaemulator/.GameActivity --es programId megabonk` |
| Guest root | `files/` inside the app's private data; reachable via `adb shell run-as com.critical.vexaemulator ...` |
| Guest rootfs libs | `files/rootfs/usr/lib/x86_64-linux-gnu/` |
| Guest thunk libs | `files/thunks/guest/libGL-guest.so`, `files/thunks/host/libGL-host.so` |
| Unity player log | `files/guests/megabonk/.config/unity3d/Ved/Megabonk/Player.log` |
| CMake | `C:\Users\zeytu\AppData\Local\Android\Sdk\cmake\3.22.1\bin\cmake.exe` |
| NDK | `C:\Users\zeytu\AppData\Local\Android\Sdk\ndk\30.0.16248370` |
| NDK objdump | `<ndk>/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-objdump.exe` |
| FEX guest build dir | `C:\Users\zeytu\Documents\FEX\build-android-arm64-ninja\Guest` (target `GL-guest`) |
| .NET | 8.0.425 SDK (also 6/7 runtimes installed) |
| Python | 3.14 at `C:\Python314\python.exe` |
| Git Bash | `C:\Program Files\Git\bin\bash.exe` — **not on PATH as `bash`** |

**Two environment traps that already cost time:**

1. `PowerShell > file` writes **UTF-16**, which silently corrupts `adb logcat` output and any
   redirected ELF/disassembly. Use `scripts/grab_log.ps1`, or redirect through Git Bash.
2. PowerShell has no `bash`. Invoke `& "C:\Program Files\Git\bin\bash.exe" -c '...'`.
   Git Bash also mangles `/data/local/tmp` style arguments in `adb push` — either pass Windows
   paths from PowerShell, or copy via `run-as ... cp /data/local/tmp/... <dest>`.

**Device gotcha:** the first launch after an APK install re-stages the rootfs from assets and
silently overwrites any guest library pushed with `adb`. Always: launch once → push → launch again.

---

## 2. The test game

| | Megabonk |
|---|---|
| Steam app | 3405340, depot 3405341 (linux) |
| Guest path | `files/guests/megabonk/` |
| Executable | `Megabonk.x86_64` |
| Unity | 2023.2.22f1 |
| Scripting | IL2CPP (`GameAssembly.so` 87,741,504 B, `global-metadata.dat` 12,643,320 B) |
| `boot.config` | `gfx-enable-gfx-jobs=1`, `gfx-enable-native-gfx-jobs=1`, `gfx-threading-mode=4` |
| Unity path used | OpenGL 4.3 core, `Renderer: Mali-G720-Immortalis MC12`, `Version: 4.3.0` |

Vampire Survivors (1794680, poncle, IL2CPP, native Linux) fails identically, so this is a
class of Unity 2022+ games, not a Megabonk quirk.

Reference games that already work: **Celeste** (MonoGame/FNA — proves the platform, the X11
fakes, and the GL thunk all work end to end), Brotato, Dead Cells, glxgears, vkcube.

---

## 3. What is proven working

Do not re-investigate these; they are ruled out.

* **Display size reaches Unity correctly.** Unity logs `Desktop is 2712 x 1220 @ 60 Hz`, and
  Unity's own SDL state (read live from guest memory, see §6) holds
  `desktop_mode = 2712 x 1220 @ refresh 60`.
* **Unity acts on it**: `requesting resize 2712 x 1220` → `Using native desktop resolution 2712
  x 1220` → `create_window 2712x1220` → `map_window` → `get_window_attributes -> 2712x1220`.
* **Surface is correct**: `[VEXA][GLX] surface ready ... gles 3.2`, fallback blit target
  `-> surface 2712x1220`.
* **Shaders compile and link**: ~56 programs, all `status=1`.
* **The render loop is alive**: 4480 swaps / 9740 draws / 14148 clears in a 55 s run. Only
  **20** `drawfail` events total, mostly `GL_INVALID_ENUM` (0x500) raised by our own
  diagnostic probes, not by Unity.
* **Game C# runs**: Steam, Rewired, saves, settings, `LocalizationPrewarmer`, 22,669 objects
  loaded.
* **Not causes**: `GLX_MAX_BUFFER_SIZE` / `glXGetFBConfigAttrib` (Unity never calls it),
  `GL_MAX_*` limits, `-screen-width/height` (1280x720 and 2712x1220 behave identically),
  32x32 probe window, advertising/hiding `GL_ARB_compute_shader`, `gfx-enable-gfx-jobs`,
  `-screen-quality`, fullscreen_mode in the save file, filling the CRTC `npossible`/`possible`
  list, setting DISPLAY/desktop size (fixed earlier), X11 symbol availability (all 131 symbols
  Unity `dlsym`s are exported).
* **Not causes**: the SIGILL/SIGBUS noise is normal FEX atomic-fixup behaviour. shaderc is not
  built and is not the issue. The Mali `GL_INVALID_ENUM` driver warnings are cosmetic.
* `git status` on **both** repos is dirty with this session's work. **Nothing is committed or
  pushed.**

---

## 4. The crash: `IndexOutOfRangeException` — fully explained

This is a *separate* bug from the black screen and it is now fully explained.

IL2CPP decompilation (`Il2CppDumper 6.7.46`) of Megabonk:

```csharp
// Assets.Scripts.Settings___Saves.SaveFiles.ConfigSettingsUtility::GetMyResolutions()
// RVA 0x2229400
var list = new List<Resolution>();
foreach (var r in Screen.get_resolutions())              // UnityEngine.Screen::get_resolutions
    if (!list.Any(e => e.width == r.width && e.height == r.height))
        list.Add(r);
return list.ToArray();                                    // empty in -> empty out

// CurrentSettings::SetResolution(int index)   RVA 0x2196D40
var res = ConfigSettingsUtility.GetMyResolutions();
if ((uint)index >= res.Length) throw new IndexOutOfRangeException();   // 0 >= 0
Screen.SetResolution(res[index].width, res[index].height, refreshRate, fullscreenMode);

// CurrentSettings::DoUpdateResolution coroutine, state machine MoveNext RVA 0x2197A00
//   at 0x2197b5d:  cmpl 0x18(%r12), %ebx ; jae 0x2197d35   -> throw
//   0x2197d35 calls il2cpp raise-exception; 0x2197d30 is the null-reference path
```

`Player.log` matches exactly:

```
IndexOutOfRangeException: Index was outside the bounds of the array.
  at CurrentSettings+<DoUpdateResolution>d__17.MoveNext ()
  at CurrentSettings.OnSettingUpdated (System.String name, System.Object value, System.Object oldValue)
  at CurrentSettings.UpdateSave ()
CurrentSettings:OnSettingUpdated(String, Object, Object)
CurrentSettings:UpdateSave()
```

**Root cause:** `Screen.resolutions` comes back empty.

### Why it is empty

`UnityPlayer.so` is stripped (398 dynsyms, all `UND`), but the scripting binding table is
intact: `UnityEngine.Screen::get_resolutions_Injected` is at file offset **0x92e8** in the
table `[{const char* name; void** thunk; int size}, ...]` starting around 0x9120. The
implementation is `DisplayManager::GetAvailableResolutions` at **vaddr 0xde2700**:

```asm
de2700:  pushq %rbp / %r15 / %r14 / %r12 / %rbx
de2708:  movq %rdi, %r15
de270b:  leaq "Error getting num native displays: %s\n"(%rip), %r14
de2712:  xorl %ebp, %ebp
de2720:  callq 0x18b2ee7              ; SDL_GetNumVideoDisplays()
de2725:  movl %eax, %ebx
de2729:  jg   de273d                  ; n > 0 -> continue
de272b:  callq 0x1884466              ; SDL_GetError()
de2738:  callq 0x15b64d0              ; log
de273d:  cmpl %ebx, %ebp ; jge de27a9 ; display >= n -> done
de2741:  ...                          ; push a new 0x50-byte entry
de278f:  movl %ebp, %edi
de2791:  callq 0xde2640               ; GetDisplayInfo(display, entry)
de2799:  testb %al, %al ; jne de2720  ; ok -> next display
de279d:  jmp  de27ad                  ; fail -> cleanup, return
```

`GetDisplayInfo` at **0xde2640**:

```asm
2654:  movl %ebp, %edi
2656:  callq 0x18b2f9a                ; returns qword from display[0x00]
265b:  leaq 0x28(%rbx), %rdi
2662:  callq 0x7a9f90                 ; free/assign that
266c:  callq 0x18b33d5                ; copies 32 bytes from display[0x18] -> SDL_DisplayMode
2671:  testl %eax, %eax ; jne de26c8 ; fail -> return false
```

`SDL_GetNumVideoDisplays` at **0x18b2ee7** (reads `_this` from vaddr `0x1fb5138`,
`num_displays` at `+0x2d0`):

```asm
18b2ee8: movq 0x702249(%rip), %rax   # 0x1fb5138   ; SDL's `_this`
18b2ef2: je   18b2efc
18b2ef4: movl 0x2d0(%rax), %ebx      ; num_displays
18b2efc: leaq "Video subsystem has not been initialized"(%rip), %rdi
```

`SDL_GetNumDisplayModes`-like function at **0x18b2f9a** loads a **qword** at
`display[0x00]` and returns it (`movq (%rax,%rcx), %rbx`, stride `0x78`) — i.e. it is *not*
a simple int count at `+0x08`; that earlier guess was wrong.

The mode copy at `0x18b33d5` reads `display + 0x18`, which holds a valid
`SDL_DisplayMode` (`format=0x17401803, w=2712, h=1220, refresh=60`), so desktop mode is fine.

### Live proof: SDL reports a display with zero video modes

`libX11_fake.c` now probes SDL's live state from **inside the guest**:

* `dladdr(__builtin_return_address(0), &info)` → `dli_fbase` gives the UnityPlayer.so load bias.
  (Do **not** scan for the ELF magic — FEX does not page-align PIE mappings. Do **not** use
  the guest's `/proc/self/maps` — FEX synthesises a minimal one showing only the main
  executable header page at 0x00200000.)
* Read `SDL _this` at `base + 0x1fb5138`, `num_displays` at `+0x2d0`, `displays` at `+0x2d8`,
  stride `0x78`.

Output:

```
[VEXA][SDL] probe: pc=0x7304b306c1 -> .../UnityPlayer.so base=0x7303265000
[VEXA][SDL] probe(XCreateWindow) base=0x7303265000 _this=0x481640
[VEXA][SDL] num_displays=1 displays=0x431c20
[VEXA][SDL] display[0] @0x431c20 raw:
  +0x00  60 38 48 00 00 00 00 00 00 00 00 00 00 00 00 00  | 0000000000483860 0000000000000000
  +0x10  00 00 00 00 00 00 00 00 03 18 40 17 98 0a 00 00  | 0000000000000000 00000a9817401803
  +0x20  c4 04 00 00 3c 00 00 00 c0 15 5a 0c 90 b3 34 00  | 0000003c000004c4 0034b3900c5a15c0
  +0x30  90 37 48 00 00 00 00 00 03 18 40 17 98 98 0a 00 00  | ...
[VEXA][SDL] display_modes=0x483860 num_display_modes=0
```

`num_displays = 1`, desktop/current mode both `2712x1220 @60`, and the mode **list is empty**.

### Why the mode list is empty

`libXrandr_fake.c` now logs `__builtin_return_address(0)` on every entry point. The complete
X11/SDL init sequence observed is:

```
XRRQueryVersion                    (CheckXRandR)
XRRGetOutputPrimary                (X11_InitModes_XRandR)
XRRGetScreenResourcesCurrent  -> tail-jumps into XRRGetScreenResources
XRRGetOutputInfo                   (X11_AddXRandRDisplay)
XRRGetCrtcInfo                     (X11_AddXRandRDisplay)
XRRGetCrtcInfo                     (SetXRandRModeInfo)
XRRGetScreenResourcesCurrent       (second looking_for_primary pass, no outputs matched)
```

That is exactly SDL2's `X11_InitModes_XRandR` (two passes of `looking_for_primary = 1, 0`).

**SDL's `X11_GetDisplayModes` is never entered.** It would issue its own
`XRRGetScreenResources` + `XRRGetOutputInfo` pair; neither appears. Xinerama and XF86VidMode are
also never consulted (both fakes now log; neither fires). So Unity's bundled SDL relies on
modes being filled during `SDL_VideoInit`, and that pass adds nothing.

Open sub-question: the mode struct carries a trailing double (`0x0034b3900c5a15c0` ≈ 0.0),
which is SDL3's `SDL_PixelDensity`. Unity's bundled SDL is therefore likely **SDL3, not
SDL2**. Both are non-lazy in `SDL_GetNumDisplayModes`, so the missing step is the same, but
the struct layout differs and that matters for writing the field directly.

### Failed experiment — do not repeat blindly

Writing a valid mode list by hand from the fake was tried: copy `display+0x18` into
`display[0x00]`'s array and set `*(int*)(d+0x08) = 1`, `*(int*)(d+0x0c) = 32`.

```
[VEXA][SDL] seeded display_modes[0] -> 2712x1220, num_display_modes=1
IndexOutOfRangeException        <- still thrown
```

So either `+0x08` is not the count (consistent with `0x18b2f9a` reading a **qword** at `+0x00`),
or `+0x00` is `driverdata` rather than the modes array, or Unity caches the display list. This
code is currently disabled behind `if (0 && ...)` in `vexa_probe_sdl`; re-enable deliberately.
The late re-read probe in `XPending` (ticks at 600) never fired — `XPending` is evidently not
called that often; pick a different per-frame hook if you need it.

---

## 5. The render blocker: 256x256 backbuffer vs 2712x1220 surface

```
[VEXA][X11] create_window -> 1 at 0,0 32x32 depth=24        <- Unity's first (probe) window
[VEXA][X11] configure 2712x1220
[VEXA][X11] create_window -> 1 at 0,0 2712x1220 depth=24
[VEXA][PRESENT] fallback fbo=2 att_type=0x1702 name=1118 256x256 draws=1 -> surface 2712x1220
[VEXA][PRESENT] fbo2 src nz=0 err=0x0: 000000 000000 000000 000000 000000 000000 000000 ...
```

Per-frame trace:

```
bindfbo fbo=4 -> clear 0x4500 -> draw 6 verts
bindfbo fbo=1 -> clear 0x4000
bindfbo fbo=2 -> clear 0x4000 -> draw 6 verts
swap dfbo=2
```

Measured target sizes: `fbo2 = 256x256`, `fbo3 = 128x128`, `fbo4 = 100x100`. 256x256 is
square, so it cannot be a scaled 2712x1220. **No geometry draw is ever recorded**, and the
final target is pure black (`nz=0`, every pixel `000000`).

Notable: Unity logs

```
requesting resize 2712 x 1220
Using native desktop resolution 2712 x 1220
requesting fullscreen 2712 x 1220 at 0/1 Hz        <- 0 Hz refresh, candidate 0 of 1
```

Unity is picking a resolution candidate with **0 Hz**. Combined with §4 (SDL reports zero
video modes), the chain *"fakes report no modes -> Screen.resolutions is empty/degenerate ->
Unity never applies a real resolution -> stays on a 256x256 boot target"* is plausible but not
yet proven. Fixing the mode list is the cheapest way to test it.

---

## 6. Tooling added in this session

| Path | What it does |
|---|---|
| `scripts/build_x11_fakes.sh` | Builds every guest X11 fake for x86_64 against `guest-tests/graphics/rootfs`. **Several sonames need more than one translation unit** — `libX11.so.6` = `libX11_fake.c` + `libX11_sdl_stubs.c`, `libXrandr.so.2` = `libXrandr_fake.c` + `libXrandr_sdl_stubs.c`. Dropping a TU silently removes exported symbols and SDL reports `x11 not available` with no other symptom. `libXinerama.so.1` must be single-TU (`*_sdl_stubs.c` duplicates symbols). |
| `scripts/grab_log.ps1` | `adb logcat -d -v threadtime` capture as UTF-8. |
| `guest-tests/x11-fake/libXxf86vm_fake.c` | Real `libXxf86vm.so.1` (was an all-NULL stub). `libXxf86vm_stub.c` is superseded. |
| `libXrandr_fake.c` | `VEXA_RANDR_TRACE` now prefixes `__builtin_return_address(0)`. |
| `libX11_fake.c` | `vexa_find_unity_base()` (dladdr) + `vexa_probe_sdl()` (reads SDL's display list from inside the guest). Seed code disabled behind `if (0 && ...)`. |
| `scripts/x11_symbol_audit.py` | Compares X symbols Unity `dlsym`s against what the fakes export. |

IL2CPP / disassembly scratch scripts in `.il2cpp/` (untracked, gitignored-ish — do not commit):

* `il2cpp_lookup.py addr <hex...>` / `name <substr>` — resolve RVA→method name via Il2CppDumper's `script.json`.
* `ann.py <asm...>` — annotate a `llvm-objdump` listing with RVA→name.
* `annotate_unity.py <so> <start> <end> [out]` — disassemble + annotate rip-relative string refs.
* `locate_unity_display.py <so> <guest addrs...>` — guess a guest load base from anchor strings.
* `build_rvas.py` — build the `rvas.json` map used by `ann.py`.

`.il2cpp/` also holds the pulled `GameAssembly.so`, `global-metadata.dat`, `UnityPlayer.so`,
`dump.cs` (31 MB), `script.json` (86 MB), and the captured `Player*.log` runs.

---

## 7. Reproducing a run

```powershell
# build the fakes
& "C:\Program Files\Git\bin\bash.exe" -c 'cd /c/Users/zeytu/Documents/vexa && bash scripts/build_x11_fakes.sh libX11.so.6 libXrandr.so.2 libXxf86vm.so.1'

# push (separate adb calls; do not chain with && inside adb shell)
adb push guest-tests\x11-fake\libX11.so.6     /data/local/tmp/libX11.so.6
adb push guest-tests\x11-fake\libXrandr.so.2 /data/local/tmp/libXrandr.so.2
adb shell "run-as com.critical.vexaemulator cp /data/local/tmp/libX11.so.6     files/rootfs/usr/lib/x86_64-linux-gnu/libX11.so.6"
adb shell "run-as com.critical.vexaemulator cp /data/local/tmp/libXrandr.so.2 files/rootfs/usr/lib/x86_64-linux-gnu/libXrandr.so.2"

adb shell am force-stop com.critical.vexaemulator
adb shell am start -n com.critical.vexaemulator/.GameActivity --es programId megabonk
Start-Sleep -Seconds 50

# grab Unity's own log
& "C:\Program Files\Git\bin\bash.exe" -c 'cd /c/Users/zeytu/Documents/vexa/.il2cpp && adb exec-out run-as com.critical.vexaemulator cat "files/guests/megabonk/.config/unity3d/Ved/Megabonk/Player.log" > Player.log'
```

Sanity check after any fake rebuild — SDL fails opaquely if symbols go missing:

```powershell
python -c "import subprocess;o=subprocess.run([r'<ndk>\\toolchains\\llvm\\prebuilt\\windows-x86_64\\bin\\llvm-readelf.exe','--dyn-syms','--wide',r'guest-tests\x11-fake\libX11.so.6'],capture_output=True,text=True).stdout;print(sum(1 for l in o.splitlines() if (p:=l.split()) and len(p)>=8 and p[3]=='FUNC' and p[6]!='UND'),'exported FUNC')"
```

Expected: **~206** for the current source (the staged reference build
`guest-tests/graphics/rootfs/usr/lib/x86_64-linux-gnu/libX11.so.6` has 202).

---

## 8. Recommended next steps, in order

1. **Find the real `num_display_modes` offset in Unity's SDL3 `SDL_VideoDisplay`.**
   `0x18b2f9a` returns the qword at `display[0x00]`. Disassemble the X11 driver's
   `X11_GetDisplayModes` — its address is in the `SDL_VideoDevice` vtable (visible in the probe
   dump as `dev+0x00..dev+0x58`, all code pointers) — and read the field offset it uses for the
   mode count and the mode array. Then re-enable the seed in `vexa_probe_sdl` with the correct
   offsets, ideally by calling the driver function pointer rather than poking fields.
   *Caution:* `dev+0x18` produced different file offsets across runs (`0xd04069` vs `0x12c4069`),
   so the vtable guesses from the probe dump are not yet trustworthy. Resolve them properly.
2. **Verify with `requesting fullscreen ... at <hz>/<n>`** whether a non-zero refresh rate and a
   non-empty candidate list appear. That is the cheapest signal that the fakes now feed Unity a
   usable resolution list.
3. **Then re-check the backbuffer size.** If Unity applies a real resolution and the target
   becomes 2712x1220, the 256x256 problem is downstream of this and largely solved. If the
   target stays 256x256, that is a genuinely separate Unity-side issue (URP `RTHandle` /
   Dynamic Resolution, UUM-58686 class) and should be pursued on the engine side.
4. Only after that, consider whether the `drawfail err=0x500` entries are worth chasing — they
   are 20 in ~10k draws and mostly come from our own probe code.

Reference reading already gathered: SDL2 `SDL_x11modes.c` (`X11_InitModes_XRandR`,
`X11_AddXRandRDisplay`, `X11_GetDisplayModes`, `SetXRandRModeInfo`) and `SDL_video.c`
(`SDL_GetNumDisplayModesForDisplay`, `SDL_AddDisplayMode`, `SDL_GetDisplayMode`) from
`https://raw.githubusercontent.com/libsdl-org/SDL/release-2.30.0/...`. Unity's Linux player
bundles SDL statically and `dlopen`s `libX11.so.6`, `libXrandr.so.2`, `libXinerama.so.1`,
`libXxf86vm.so.1`, `libXi.so.6`, `libXcursor.so.1`, `libXext.so.6`, `libXss.so.1`,
`libxkbcommon.so.0`, `libwayland-client.so.0`, `libvulkan.so.1`.

---

## 9. Constraints and preferences

* Never ask for or handle the Steam password. The user runs `login <steamid>` interactively;
  SteamCMD cached credentials expire and scripted `+login` fails with `Invalid Password`.
* Steam app 2757760 ("Summer at the Edge of the Universe", real title *Samayuni: Curiosity*)
  is Unity **2021.3.33f1 Mono** with a clean `boot.config` (no gfx-jobs flags) and exists
  locally at `C:\Users\zeytu\Games\dl\summer` — but only the **Windows** depot (2757763) was
  downloaded. Re-run `app_update 2757760 -os linux` to get a native Linux build. Useful as a
  Unity-2021 reference; `unify` is not viable for IL2CPP titles (it ships only Mono player
  variants and would need a 5-8 GB Unity Editor download).
* The user prefers decisive action over long option lists, and explicitly authorised deep
  internet research, Unity decompilation, and step-by-step/gdbserver-style debugging.