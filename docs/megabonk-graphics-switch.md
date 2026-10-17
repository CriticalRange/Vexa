# Megabonk graphics switch

`VEXA_GRAPHICS_BACKEND=gl|vulkan` selects the Unity renderer for the native Linux Megabonk build. `gl` is the default when unset and preserves the existing GLCore path. `vulkan` replaces `-force-glcore` with `-force-vulkan`. Invalid values fail before FEX starts. This does not select Proton or the real X11 server.

For a later launcher UI, pass `GameActivity.EXTRA_GRAPHICS_BACKEND` (`"graphicsBackend"`) with `"gl"` or `"vulkan"` on the Megabonk intent. It becomes `VEXA_GRAPHICS_BACKEND` in the launch environment. The selected value applies to that launch only; nothing is saved globally.

Headless launches accept the same env variable, for example:

```powershell
adb shell am start -n com.critical.vexaemulator/.runtime.HeadlessLaunchActivity --es programId megabonk --esa env VEXA_GRAPHICS_BACKEND=vulkan --ei ttlMinutes 30
```

Headless `graphicsBackend` intent extra remains supported and takes precedence over the env variable. `graphicsBackend=gles-surface` continues to select the GL surface diagnostic path. Headless Vulkan selects the Android render surface. Do not infer that Vulkan is faster solely from the API choice; compare the same gameplay scene, resolution, frame cap, and thermal state.

On 2026-10-16, a headless menu run on the connected Mali-G720 device at 2712×1220 measured about 8.32–8.72 ms per GL swap (roughly 115–120 FPS). Vulkan initialized successfully, but its offscreen ImageReader delivered images at roughly 13.9–15.5 ms intervals. Those are different timing metrics and do not establish a controlled gameplay comparison. GL reported swap interval 0, so the observed ~120 FPS was not a VSync limit. Removing the periodic non-trace `glGetError` diagnostic from the FEX GL thunk did not yield a measurable menu FPS increase. Logs and CPU samples are in `logs/megabonk-20261016/`.
