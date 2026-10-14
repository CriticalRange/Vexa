# Megabonk rendering, 2026-10-14

Device: Xiaomi 2407FPN8EG, Mali-G720 Immortalis. Tests use HeadlessLaunchActivity and EGL pbuffer rendering. Current ADB transport is 192.168.8.13:44715; discover devices before reuse. Screen timeout was temporarily changed from 30000 to 2147483647 ms at the user request. Original display settings are recorded in logs/megabonk-render-20261014/device-display-settings.json.

## Dark image: confirmed color-space mismatch

The final framebuffer reports GL_SRGB, but the default EGL surface reported GL_LINEAR while GL_FRAMEBUFFER_SRGB was enabled. Blitting decoded the source into linear light without re-encoding it for display.

The Android GPU regression probe (guest-tests/graphics/srgb_blit_probe.c) reproduces the exact path using an sRGB source framebuffer: linear gray 0.18 produces byte 46 on a linear destination and the expected byte 118 on an sRGB destination. Both passes have zero GL errors; SRGB_BLIT_TEST_OK.

The Megabonk profile now enables VEXA_GL_SRGB_SURFACE=1. Runtime launch mirrors this explicit control into the host environment, not just guest envp. GLX capability reporting, initial EGL surface creation, and window surface rebuilds honor the same control. Other profiles keep their previous surface selection.

Before: color-linear.log shows read=20 encoding=8c40, draw=0 encoding=2601, srgb_write=1.
After: color-srgb-native.log shows both source and destination encoding=8c40, srgb_write=1. scene-srgb.png is the actual corrected offscreen game frame: terrain, trees, enemies, and grass no longer have crushed midtones. No shader brightness multiplier was added.

## Independent unsupported texture formats

R16 and RGBA16 storage was rejected; EXT_texture_norm16 is absent. R16 lookup uploads and RGBA16 render targets now use supported float32 storage where EXT_color_buffer_float exists. Integer normalized uploads are converted to [0,1] floats with unpack/PBO state preserved. This also removed the invalid framebuffer errors and restored minimap markers. These errors were separate from the final display transfer that caused the dark scene.

## Grass

The original grass shader exceeded the driver's zero vertex storage-block limit. The readonly 64-byte HLSLCC transform records can be read from the same GPU buffer through RGBA32UI texture-buffer fetches. The virtual storage-block name/binding reflection lets Unity bind its existing buffer. Runtime logs show the real transform buffer used by grass draws; leaves are visible in scene-srgb.png.

Grass quality 2 was used for controlled testing; the prior quality value was 0. The original config is saved in config-grass-test-before.json. Other save/progression fields are not reset.

## Verification and remaining scope

Debug APK assembled and installed. Native sRGB blit probe passed. Final sampled log has zero rejected TexStorage formats, zero invalid-framebuffer draw errors, and zero vertex storage-block link errors. The existing desktop texture LOD-bias/enable enum diagnostics remain; this report does not claim all OpenGL compatibility work is complete. Multi-window geometry was investigated separately and is not part of the installed darkness fix.

The checkpoint commits before this rendering work are VEXA a209e50 and FEX 2e5330723. Evidence is in logs/megabonk-render-20261014/.

## Draw-count investigation

The five-million value was a lifetime counter, not a per-frame count. A captured upgrade-screen frame contains 489 guest draw submissions: 389 to FBO 16, 71 to FBO 20, and 29 across smaller intermediate targets. The frame ends with one color blit to the default surface. Instanced calls remain instanced; each draw wrapper forwards once to the corresponding native draw. No duplicate submission by the bridge was found in this capture. This does not prove that all engine-side passes are optimal.

The frame-target diagnostic queried GL_VIEWPORT on every offscreen draw, although the viewport fields were only printed in a bounded diagnostic and never used to select or present the target. Removed these queries and fields; framebuffer draw counts and actual attachment dimensions still drive fallback selection. The captured frame therefore avoids 489 extra viewport queries without discarding any guest rendering.

Compute dispatches previously called VexaDiagDraw, incorrectly incrementing the draw counter, preparing a grass buffer, and marking the bound framebuffer as drawn. They now have a separate dispatch counter and do not affect raster presentation tracking. The sampled Megabonk frame has no compute dispatch, so this correction does not explain its 489 draws.

The periodic log now reports draws/frame and dispatches/frame over the last 128 swaps. The partial thunk trace count is labelled traced_calls_total rather than the misleading calls/frame. Slow-frame logs now subtract the previous frame's counters instead of reporting identical cumulative values twice.

Both host thunk widths built successfully; the debug APK was assembled and installed. Gameplay verification evidence follows below. Temporary grass quality 2 was restored to its original value 0, preserving the other configuration fields. Direct FPS comparisons with the earlier upgrade frame are not controlled benchmarks because the scene and quality differ.

Final installed-build capture (`draw-frame-after.txt`) records 310 submissions in active gameplay: 263 to FBO 16, 18 to FBO 20, and 29 to intermediate targets. `draw-scene-after.png` shows gameplay at 0:19 with 122 displayed FPS. Later 128-frame windows report approximately 361 draws/frame as the scene/UI changes; dispatches remain zero. The last sampled 256-frame window reports average 7.42 ms, p99 13.30 ms, max 16.72 ms, with zero frames over 33 ms in that window. These numbers are observations, not a measured improvement percentage or a promise for every scene.

`draw-final.log` confirms sRGB-to-sRGB presentation, a valid grass transform buffer, and no invalid-framebuffer or vertex-storage link failures. Slow-frame lines now contain actual frame-local counts (for example draws=366, useprog=126, bindfbo=23). Both the 64-bit and 32-bit host builds passed; gameplay verification here exercises the 64-bit Megabonk path. Diagnostic request flags were removed after capture; the game remains headless and the requested awake setting remains active.
