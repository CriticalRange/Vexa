# Super Meat Boy: device-verified launch and first-level rendering

Verified 2026-10-23 on Xiaomi 2407FPN8EG, Android 16, Mali/MT6989.

## Findings

The original claim that ARB assembly-program translation is required was not
supported by the failing shader. The game contains MojoShader GLSL and GLSL120
backends and recognizes `-mojoshaderprofile=glsl120`. A live, pause-free
`qVexaThreads`/memory probe confirmed the active context profile was `glsl120`
and core GL support was enabled.

1. The actual first shader failure was `Undeclared variable 'gl_TexCoord'`.
   `files/artifacts/shaders/failed_1.glsl` contained an ESSL100 fragment shader
   with `#define ps_t0 gl_TexCoord[0]`. Existing desktop-to-ES rewriting changed
   its version/precision but did not provide desktop compatibility varyings.
2. After fixing that shader, the game crashed while filling font vertices.
   The guest stack contained `OpenGLLockVertexBuffer+0x91` (return at 0x55e461)
   followed by `FontEmitter::FillVertexBuffer`. The game uses ARB buffer names;
   these lacked both host routing and guest invoker aliases. Consequently
   `glMapBufferARB` returned zero and the game wrote through a null pointer.
3. After fixing the host AND guest buffer aliases, drawing progressed but the
   native fullscreen dimensions caused a game viewport/scissor underflow:
   `0,2147483495 2712x1525`. Disassembly of `gfxAutoSetViewport` showed logical
   right-shifts of potentially negative dimension differences. Running windowed
   at 1280x720 avoided this game arithmetic bug and produced visible output.

The missing-procedure fallback logs the shared last missing name, so repeated
`glProgramLocalParameterI4ivNV` invocation messages do not identify the actual
missing function being called. Also, `MOJOSHADER_glGetError()` retained a stale
ARB capability error despite the actual GLSL compiler failure. The driver shader
dump was the decisive evidence.

## Changes

In sibling `../FEX/ThunkLibs/libGL/`:

- `legacy_shader_varyings.inc`: map old texture-coordinate, primary/secondary
  color and fog varyings to matching ESSL declarations across shader stages.
- `libGL_Host.cpp`: apply that mapping to legacy shader rewrites; route ARB
  buffer names to core functions and the existing buffer-map wrapper.
- `libGL_Guest.cpp`: map the same ARB buffer names to existing guest invokers.

Built `GL-host-64` and `Guest/GL-guest` successfully. Deployed both libraries
together into device `files/thunks/{host,guest}` and copied the same outputs to
`app/src/main/assets/runtime/thunks/{host,guest}` for future APK staging.
No APK was rebuilt or installed in this investigation. 32-bit payloads were
not rebuilt, and other games have not been regression-tested.

Device custom-program record `supermeatboy` now has these arguments:

```
-mojoshaderprofile=glsl120 -windowed -1280x720
```

Other custom-program records and the existing environment were preserved.
The pre-existing Steam/SDL and four no-op executable patches were retained.
A temporary diagnostic patch exposing MojoShader's error was removed by atomic
replacement with the exact executable present at the start of this investigation.
The temporary gdb flag and TCP forward were removed.

## Verification and files

Observed opening animation, title, main menu, chapter selection, level selection,
and level 1-1. Touch overlay navigation worked. In level 1-1, holding right moved
Meat Boy and pressing A made him jump; screenshots show the position change and
airborne character. The user confirmed audible sound. No game-wide performance
claim or full-playthrough claim is made.

Local evidence and backups: `scratch/smb/`:

- `gameplay.png`, `gameplay-input.png`: first level and movement/jump evidence.
- `fex_stderr.log`, `run_manifest.json`, `probe.log`: captured run evidence.
- `SuperMeatBoy.current`: executable before this investigation.
- `custom-programs.before.json`: original device custom-program configuration.
- `libGL-host.before.so`, `libGL-guest.before.so`: original device GL pair.
- `libGL-host.asset-before.so`, `libGL-guest.asset-before.so`: original APK assets.

SHA-256 of installed and staged outputs:

```
host  05a323fa41678728c32c902da0a27b4bf762a637ea93af576d3dd8a701f6ff82
guest 7c0437ffc346c7a27749bdb79161cae309dde1e1691fe4f6c67b6c2d1af0aca0
game  d84417b31382d892d9280100fb3f4cb8832b8d3fc46f18b27e55e47305495919
```

To roll back, stop VEXA before restoring the original device GL pair and custom
program JSON from those backups; restore APK assets separately if needed. Do not
overwrite/truncate a mapped library while a guest is running.

MojoShader's upstream [profile documentation](https://github.com/icculus/mojoshader/blob/main/mojoshader.h)
distinguishes GLSL output from ARB vertex/fragment program output. The conclusions
above rely on the actual local executable, device logs, shader dump and input test.
