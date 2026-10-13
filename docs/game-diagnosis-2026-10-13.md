# Game diagnostics, 2026-10-13

Device: Xiaomi 2407FPN8EG, Mali-G720-Immortalis MC12. Evidence is under
`logs/diagnosis-20261013/`. Previous installed APK is `before-overlay.apk` there.

## Dead Cells

The original fatal SIGSEGV was reproduced in fresh processes. The fault was eight
bytes below the guest's writable 8 MB stack. Live FEX register/stack sampling
showed a repeating OpenAL / ALSA chain, not unusually large legitimate stack use:

`bundled libopenal -> VEXA libasound -> dlopen(libopenal.so.1) -> bundled libopenal`.

`LD_LIBRARY_PATH=.` caused relative opens with AT_FDCWD to bypass FEX's absolute
thunk overlays. FileManagement now matches cwd-relative opens against those same
overlays. Paths containing `..` are left alone to preserve symlink semantics.
The original game files and stack allocation are unchanged.

After this fix, `LoadLib: libopenal` appears and the stack overflow disappears.
Subsequent errors exposed independently missing compatibility behavior:

- Desktop GLSL 150 was passed straight to GLES. Convert the older modern GLSL
  versions through the existing ESSL 300 path.
- The legacy identifier `sample` is reserved in ESSL. Rename complete tokens.
- Heaps' MRT outputs OUTPUT1..OUTPUTn require locations 0..n-1 in ESSL.
- OpenAL extension lookup returned host ARM code addresses to x86 callers.
  AL/ALC lookup now checks host availability and returns generated guest
  entrypoints. Unknown functions return NULL; ALC handle conversion is retained.

The engine reached the title screen and started Prisoners' Quarters after these
fixes. The low-pass audio crash disappeared. An additional framebuffer problem
initially prevented the level from rendering: FBO 1 returned
FRAMEBUFFER_INCOMPLETE_ATTACHMENT (0x8cd6). Color attachments were RGBA8 textures
of matching dimensions; the depth/stencil renderbuffer was the remaining suspect.
The renderbuffer storage call used unsized GL_DEPTH_STENCIL, accepted on desktop
but requiring a sized format on GLES. Mapping it to GL_DEPTH24_STENCIL8 fixes
the attachment. Device logs confirm that exact conversion at 1280x720 and
2712x1220. Both regular and multisample entrypoints are routed to the wrapper.

Final acceptance: device screenshot shows the player, NPC, environment and
effects inside Prisoners' Quarters. The sampled draw logs continue with err=0;
no shader compile/link failure, SIGNAL 11, or err=506 appears in the final capture.
The initial fresh-save warning was followed by creation of save/user_0.dat and
successful level entry. No save files were deleted or replaced.
This verifies startup and rendered level entry, not a full playthrough, measured
FPS, or auditory quality. Other games have not received new regression runs.

GL and OpenAL host/guest payload hashes match their staged APK assets. Both
host architectures and guest thunk builds compiled; :app:assembleDebug and
git diff --check passed. Updated APK is installed on the connected device.
Evidence: `deadcells/depth-fix/`, `deadcells-screen.png`, `build-final.log`.
The FEX source diff is exported to `fex-changes.patch` in the evidence directory.

Changes to runtime/thunk sources are in the sibling `../FEX` repository.
VEXA's staging script now stages the OpenAL host/guest pair together.

## Other games and Vulkan

- Megabonk: the device Player.log explicitly reports that GrassShaderOptimized
  needs one vertex SSBO while the GLES driver permits zero. Caching that shader
  cannot remove this feature limit.
- Vampire Survivors: the current Linux build uses Mono, unlike Megabonk's IL2CPP.
  Its log has a real display and successful shader links, plus a libparty plugin
  load failure. That warning is not yet proven to cause the loading loop. Do not
  reuse the obsolete empty-resolution diagnosis without reproducing it.
- Terraria: current logs initialize FNA/SDL3 and link shaders successfully.
  They do not establish where the startup time goes. Mono JIT, asset loading,
  audio startup and GPU compile/link need separate timing before blaming shaders.

Vulkan can address graphics API limitations when the game's native Vulkan path
works or an OpenGL-on-Vulkan implementation such as Zink supports the device.
It does not remove FEX x86-to-ARM translation, Mono/HashLink JIT, audio bugs or
loader failures. Historical force-Vulkan Megabonk logs enumerate the Mali device,
but do not prove working rendered frames.

Pipeline caches reduce repeated compilation, are device/driver dependent, and
need invalidation and fallback. OpenGL program binaries can also be cached;
Vulkan is not a prerequisite for caching. No Vulkan migration or cache was
implemented in this change.

References:
- https://docs.vulkan.org/guide/latest/pipeline_cache.html
- https://docs.vulkan.org/spec/latest/chapters/pipelines.html
- https://docs.mesa3d.org/drivers/zink.html

## Follow-up: edge jitter and stepped VFX

User reported scene/character edge jitter and unusually slow-looking steam VFX.
The active game still had successful draws (err=0), so that symptom is distinct
from the earlier incomplete framebuffer. The desktop GLSL 150 source explicitly
declares `precision mediump float`. Desktop GLSL ignores precision qualifiers;
ESSL does not. The converted shader retained this declaration after the injected
highp defaults, thereby lowering the precision of later coordinate/time values.

The desktop-to-ESSL path now promotes whole lowp/mediump qualifier tokens to
highp, preserving desktop numeric precision. Already-native ESSL is unchanged.
The installed test build logs the promotion in both vertex and fragment shaders.
The level renders, with 1292 successful links, zero shader failures and zero
framebuffer errors in the after-highp capture. The user subsequently confirmed
that both the edge jitter and stepped VFX were fixed. No measured FPS improvement
is claimed.
Evidence: `deadcells/flicker/after-highp/` and its `screen.png`.

Precision semantics:
https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.1.40.pdf
https://registry.khronos.org/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.html
