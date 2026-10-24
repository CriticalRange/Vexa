# Portal legacy GL follow-up (2026-10-24)

## Verified implementation and device checks

The opt-in `VEXA_GL_CLIP_PLANES=1` path adds six context-local clip equations,
inverse-transpose MODELVIEW capture at glClipPlane time, matrix queries and
shader-side clipping. GLSL gl_ClipVertex and projected gl_Position paths are
covered separately. Fragment discard runs before the original fragment body.
This is raster clipping, not a claim of exact geometric/MSAA clip-distance parity.

Legacy CPU vertex/color/texture arrays are staged to GLES buffers for direct
draw calls, including indexed draws and double-to-float conversion. Client and
server attribute stacks preserve the tested state subset; texture object
parameters, advanced integer vertex bindings and full display-list semantics
are not a complete desktop compatibility implementation.

Both i386 and x86_64 on Xiaomi 2407FPN8EG pass the packaged APK probe chain:
36 alpha + 3 ARB shader-object + 13 ARB assembly + 14 clip + 12 legacy pixels =
78 framebuffer assertions per ABI. The legacy tests also check extension-name
visibility through both glGetString and glGetStringi, pointer-width guards,
error paths, indexed masks, centroid/MRT translation, ftransform MVP updates,
and 3D sampler compilation. Logs: scratch/portal/clip-results/legacy{32,64}-packaged-*.

## Startup blockers resolved

ToGL can report a missing function even when its pointer is non-null: its
extension-group boolean must also be true. The old GL_EXTENSIONS string did
not include desktop aliases exposed by GL_NUM_EXTENSIONS/glGetStringi.
The legacy string now includes those aliases. VBOs map to core GLES, and
GL_EXT_draw_buffers2 is advertised only with indexed-draw-buffer support.
Indexed EXT operations use native GLES operations, verified by framebuffer pixels.

Portal GLSL 1.20 uses centroid varyings and sampler3D. Its opt-in compatibility
path now emits ESSL 300, preserves centroid interpolation, converts attribute,
varying, texture calls and fragment outputs, and implements the tested
ftransform matrix behavior. Mali also requires the version directive on line
one, ahead of ToGL's stage-marker comment.

Android's native pthread shadow-call-stack mmap previously failed with ENOMEM.
The FEX 32-bit host allocator reserved nearly all high virtual address space;
bionic/Scudo/driver mappings bypass its jemalloc hooks. On Android the default
host allocator now reserves an 8–40 GiB virtual pool, keeping host pointers
above 4 GiB and leaving native mappings room. The guest retains its separate
32-bit allocator. A/B process VmSize was about 534 GB before versus 57 GB after;
RSS was around 0.5 GB, so these are virtual reservations, not physical RAM usage.
The shadow-stack abort did not recur in the subsequent Portal launch.

## Current acceptance status

Portal passes its required GL entry-point checks, initializes audio and renders
the title/menu text and textured menu scene. First-map validation and overlay
input are tracked below; these are distinct from a complete game playthrough.
No FPS improvement is claimed from the probe tests.

Build uses matched host/guest GL thunks and an updated runtime STAMP. Local APK
build bypasses the pre-existing FEX compatibility pin check and automatic thunk
staging after explicitly staging the four GL payloads. See scratch/portal/clip-apk-build.log.

## Confirmed partial mapping regression

A new i386 test uploads a VBO, maps it through glMapBufferARB(GL_WRITE_ONLY),
changes one component and draws again. Before the fix the expected red triangle
vanishes (blue clear pixels); after the fix it stays red. The shadow buffer had
been allocated without preserving the existing native data, then copied back
in full on unmap. This corrupts vertices outside a partial update.

For non-invalidating write-only maps, the adapter now initializes the shadow
through a separate native read mapping. The application's actual native map
keeps its requested access flags. This can add synchronization/copy work;
correctness is prioritized over the UNSYNCHRONIZED performance hint. Evidence:
scratch/portal/clip-results/map-preserve-before-* and map-preserve-after-*.
The expanded chain has 79 pixel assertions per ABI.

## BGRA vertex input

A captured ToGL UI draw had an enabled color attribute still bound to buffer 0,
size 2, type float, while ToGL was requesting size GL_BGRA. GLES rejected that
size with GL_INVALID_VALUE and retained the earlier input state. The adapter
now stages BGRA vertex colors into native RGBA floats for direct draws,
including unsigned bytes and signed/unsigned 2_10_10_10_REV normalization.
Attribute size queries preserve GL_BGRA, and VAO deletion drops compatibility
metadata. Pixel tests cover byte colors, indexed draws and both packed types.
Scene textures and geometry became visible in the next Portal device run;
menu/text and sustained gameplay are separate acceptance checks.

The unsigned-byte/packed BGRA paths and alpha8 texture sampling extend the chain
to 84 framebuffer assertions per ABI. GL_ALPHA8 upload was accepted by this
Mali driver; it was tested as a hypothesis and no texture-format workaround
was added. The temporary native-resolution launch did not restore the menu.

## Packed font/overlay textures

A captured menu font atlas contained only red/green data and zero alpha. The
BGRA + UNSIGNED_INT_8_8_8_8_REV upload selected VexaRepackPixel's 16-bit fallback:
it copied two bytes into a zero-initialized four-byte output pixel. This made
font/overlay sprites transparent. Both packed 32-bit orders now decode all four
components, apply byte swapping at word width, then exchange B/R as required.
The same conversion serves TexImage2D and TexSubImage2D. Before/after pixel logs
are packed-bgra-before-* and packed-bgra-after-* in scratch/portal/clip-results.
Tests include BGRA REV allocation, partial updates, non-REV packed data and
UNPACK_SWAP_BYTES. The expanded chain totals 88 framebuffer assertions per ABI.

UI stencil/culling/clip overrides and additional texture/state dumping were
investigation only and removed from the final GL source. Real clip equations
were all disabled in the captured menu draw. Source's shipped dummy vertex
buffer was 512 KiB and within bounds; no padding workaround was added.

## Final volume upload and Steam-path checks

The packed 32-bit conversion also applies to 3D volumes. Their native upload
previously retained the desktop packed type after conversion, raising
GL_INVALID_ENUM. Allocation and subimage calls now use the converted upload
type, and a null pointer with an unpack PBO is treated as byte offset zero.
The probe adds direct volume allocation, slice update and PBO-offset-zero
sampling, for 91 framebuffer assertions per ABI.

The remaining approximately 65-second menu termination was traced to guest
libsteam_api.so offset 0x32b14a, dereferencing 0x6f6c while destroying a
pointer-containing object. This is the game's bundled gbe_fork build. An
A/B launch using its supported [main::connectivity] disable_networking=1 option
passed 90 seconds without that termination. This limits the emulator's Steam
networking features for this single-player Portal setup. The setting is in
files/guests/portal/bin/steam_settings/configs.main.ini on device.
The supported option is documented in the project's
[example configuration](https://github.com/Detanup01/gbe_fork/blob/dev/post_build/steam_settings.EXAMPLE/configs.main.EXAMPLE.ini).
The temporary FEX fatal-guest-register instrumentation was removed afterward.

All four final GL payloads are compared by SHA256 across the FEX build, staged
assets, APK entries and device. Evidence is final-payload-hashes.json. Native
core build/install verification is separate from these GL payload checks.

## RGB storage and color-correction regression

An in-game console A/B changed only mat_colorcorrection from 0 to 1 with
mat_hdr_level=0: the visible first room became black. RGB8 3D storage with
BGRA + UNSIGNED_INT_8_8_8_8_REV independently reproduced GL_INVALID_OPERATION.
The packed converter retained an RGBA upload even when the native RGB8/SRGB8
storage requires RGB data. Both 2D and 3D allocation/subimage conversions now
remove the unused alpha after decoding packed components, keeping the source
row/image strides and PBO handling. Storage format and sampling semantics are
preserved. No permanent color-correction/HDR disabling workaround is installed.

The final packaged probes pass 95 framebuffer assertions per ABI (36 alpha,
3 shader objects, 13 assembly, 14 clipping, 29 legacy). The added cases cover
SRGB8 2D allocation/update and RGB8 volume allocation/slice update. Before/after
failure evidence: rgb-volume-before-* and rgb-volume-after-*; packaged results:
legacy{32,64}-rgb-packaged-*. All four GL build/assets/APK/device hashes match
in final-payload-hashes.json.

Portal now has a dedicated keyboard/mouse overlay profile (also steam-400).
The bundled i386 SDL cannot consume the x86_64 evdev preload. Fallback records
are delivered without the SDL synthetic-input suppression flag, with WASD on the left stick and separate d-pad
menu arrows, jump/Enter, crouch, use, Escape and primary/secondary mouse buttons.
Portal retains the packed relative-pointer marker for fake X11; clearing that
marker would interpret the encoded stick delta as a huge absolute coordinate.
Other profiles retain their existing defaults.
The normal menu launch configuration is restored after diagnostic map launches.

## On-device scene acceptance

The final GL payload renders testchmb_a_00 with mat_colorcorrection=1 and
mat_hdr_level=2, confirmed in the console before loading. The first room,
textured glass, fixtures and jump tutorial are visible. Evidence:
portal-final-verify-console.png and portal-final-room-color-hdr.png under
scratch/portal/clip-results. This first-map check used a temporary console
exec script to select the map; it is not proof of the complete menu-to-game
flow or every later portal/shader combination. The console/config script is
removed and normal launch arguments restored after testing.

The overlay generates direct W down/up and camera-motion records, and the
before/after first-room images show movement. Camera transport validation
is repeated after preserving the packed X11 marker. No whole-game stability
or gameplay-wide FPS conclusion is drawn from this short first-room test.

## User acceptance correction and compile audit

The user reports that camera rotation and audio do not work, performance is
poor, and many surfaces remain black. First-room visibility and pixel probe
passes therefore do not establish a solved or fully playable Portal build.
The subsequent [compiler audit](portal-shader-audit-2026-10-24.md) records 2,358
successful compile operations, 79 successful links and 276 traced draws without
GL errors. Camera rotation remains unverified/unresolved despite input records.
