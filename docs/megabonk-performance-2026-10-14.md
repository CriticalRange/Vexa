# Megabonk graphics performance, 2026-10-14

Device: Xiaomi 2407FPN8EG, Mali-G720 Immortalis. Resolution 2712x1220, saved resolution index 35, original graphics settings retained. Tests run in the background through HeadlessLaunchActivity. ADB changed from port 44715 to 38957; rediscover the transport before use.

## Results

These are sequential menu samples, not universal or controlled gameplay benchmarks. Each average below uses the last ten overlapping 256-frame reporting windows.

| Path | Mean interval | Evidence |
| --- | ---: | --- |
| Native GLES, pbuffer | 7.62 ms | perf-native-baseline.log |
| GLES through system ANGLE/Vulkan, same pbuffer | 9.276 ms | perf-angle.log |
| Native GLES, two Unity job workers | 8.462 ms | perf-job2.log |
| Native GLES, ImageReader surface | 11.662 ms between consumed images | gles-surface-present.log |
| Native Unity Vulkan, same ImageReader surface | approximately 16 ms between consumed images | vulkan-service-menu-present.log, menu portion before images=8960 |

Pbuffer swap intervals and consumed-image intervals are different measurements. Do not compare the first three rows directly with the final two. Background animation, clock scaling and sequential-run effects limit precision. ANGLE did not improve the tested menu. Reducing workers from seven to two reduced sampled CPU-clock time from 31.20 to 22.30 CPU-seconds over approximately ten wall seconds, but worsened the menu interval; the catalog therefore keeps its original worker setting.

System ANGLE was confirmed to use Vulkan 1.3.247 on the Mali GPU. Its per-application settings were backed up in angle-settings-before.json, applied only to VEXA, then restored exactly, preserving all existing package entries. angle_debug_package was originally absent and was deleted on restoration.

## Direct Vulkan now renders

Vulkan thunks and Xlib/xcb-to-Android WSI already existed. Headless Vulkan lacked an ANativeWindow. Added an opt-in ImageReader buffer-queue consumer, capture support and present-interval reporting. Start with:

```
adb -s CURRENT_SERIAL shell am start -n com.critical.vexaemulator/.runtime.HeadlessLaunchActivity --es programId megabonk --es graphicsBackend vulkan --ei ttlMinutes 30
```

The launcher replaces -force-glcore with -force-vulkan for this request only. The normal catalog continues to use GLCore. graphicsBackend=gles-surface exercises native GLES on the same offscreen window; the original headless pbuffer path remains available when this extra is omitted.

native-vulkan-menu.log reports Vulkan API 1.1 requested, driver 1.3.247, and successful Android surface creation. native-vulkan-frame.png is an actual native-resolution menu frame, not an enumeration probe. vulkan-service-select.png proves menu interaction, and vulkan-gameplay.png shows the rendered level with its upgrade overlay and 68 displayed FPS. This does not establish that every effect or controller path is correct.

The first consumer lived in the invisible UI process. ActivityManager explicitly killed that cached process for excessive CPU. The consumer now belongs to the foreground RuntimeService on its own handler thread. The corrected run kept producing tens of thousands of frames while the UI stayed invisible. Both surface APIs use this service-owned consumer for comparison. Images are released after consumption; capture uses a temporary PPM and atomic rename. The debug APK built and was installed successfully.

## Rendering hot-path cleanup

glClientWaitSync previously logged every GL_TIMEOUT_EXPIRED result, including normal zero-timeout GPU polls. The native baseline contained 27,213 clientwait lines over roughly 30,000 swaps. Normal polling now logs only the first four calls and every 2,048th call, with full opt-in tracing and wait-failure reporting preserved. Return values, flags, timeout and fence handling are unchanged. Both host thunk widths built. No isolated FPS improvement percentage is claimed for this cleanup.

## Limits and next profiling target

simpleperf recorded 3,089 native-GLES CPU samples without loss. About 21% were attributed to libvexa, with prominent futex syscall paths across seven workers; about 10% were in the Mali GLES driver. Approximately 55% were not symbolized. The recorded Vulkan sample addresses were inside FEXMemJIT mappings, so guest-code symbolization is needed before changing a purported hot game function. CPU sample percentages are not GPU utilization.

GPU devfreq access was denied. The reported Android thermal status was zero and battery saver was off. The main and render threads were observed on CPU 7 and CPU 6; this does not prove that vendor frequency management or GPU work is irrelevant.

Native Vulkan is an experimental working alternative, not a demonstrated FPS upgrade. The current measurements favor GLES. Further work should identify guest hot blocks and separate GPU execution time from CPU submission/waiting before selecting API or scheduling changes. Shader/pipeline caches principally address repeated compilation and first-use stalls; no new persistent cache was implemented here. Unity reported a missing Vulkan PSO cache on the first run; existing Android EGL shader-cache entries were also observed.

Primary references: [ANGLE Android driver selection](https://github.com/google/angle/blob/main/doc/DevSetupAndroid.md), [Unity job-worker-count](https://docs.unity3d.com/es/2019.3/Manual/CommandLineArguments.html).

## Vulkan optimization investigation on the reconnected device

Transport changed to 192.168.1.160:44413. Continued with native Vulkan in HeadlessLaunchActivity at 2712x1220, keeping the screen awake. No persistent worker-count or single-threaded rendering change was made.

Fixed FEX's Android JIT symbol output: the app UID cannot create /data/local/tmp/perf-PID.map. FEX now accepts FEX_JIT_SYMBOL_DIR; VEXA points it to its private files directory only when fex_jitnames.flag is present. A real 72 MB map was produced and pulled. The startup profile attributed about 67% of samples to two Unity texture-decoding blocks (offsets a22840 and a21d30); these are startup work, not evidence of a steady rendering loop. The subsequent steady menu profile attributed about 48% to UnityPlayer, 18% to the FEX futex syscall path and 1.7% to the Vulkan guest library. See vk-steady-summary.txt. The offsets are JIT block entries, not independently identified function boundaries.

Found and corrected a Vulkan dispatch bug: LookupCustomVulkanFunction did not route vkCreateSwapchainKHR or vkGetPhysicalDeviceSurfaceCapabilitiesKHR to their existing Android implementations. Calls obtained through vkGetDeviceProcAddr bypassed extent/transform/composite-alpha adaptation. The corrected route is proven by a successful native-size swapchain log. This is a compatibility correction; it does not establish an FPS gain.

Added opt-in per-thread host wall-time reporting for acquire, present and fence waits (fex_vkprofile.flag). It measures time spent inside the native API, including waits, not GPU execution time. Results vary across runs: acquire was roughly 1.1-1.3 ms and present roughly 2-5 ms. Logs are emitted per 256 calls. Normal launches skip the clocks.

The corrected swapchain reported six images, FIFO mode 2 and format 43. A separate opt-in fex_vkmailbox.flag test queried the actual surface's advertised modes and selected MAILBOX only when supported. Both modes rendered. The last twenty overlapping menu windows averaged FIFO 17.113 ms and MAILBOX 16.554 ms; sequential-run variability is too large to claim a repeatable gain. MAILBOX override was disabled after the test. These are consumed-image intervals and can also be affected by frame dropping in MAILBOX.

Additional experiments did not establish a stable gain: four job workers averaged 13.028 ms versus 12.049 ms in the earlier seven-worker GPU-only-surface sample. The CPU-readable-surface requirement was temporarily removed for that diagnostic and then restored; the experiment is not a shipped mode. -force-gfx-mt selected kGfxThreadingModeThreaded and averaged 15.455 ms across seventeen windows, but was not promoted from this sequential test. -force-gfx-direct caused a real SIGSEGV during startup and was removed. Vulkan normally selects kGfxThreadingModeClientWorkerNativeJobs. See vk-optimization-comparison.json and the corresponding logs. Profiling/MAILBOX flags were removed, retaining the normal native Vulkan rendering mode.

No pipeline-precompilation feature was added and no Vulkan FPS improvement is claimed from these experiments. Remaining work requires comparable gameplay profiling and GPU execution timing; CPU samples and API wall times alone do not isolate GPU shader cost.

Final verification: the cleaned debug APK built and installed successfully. Device host payload SHA256 matches the staged/build payload (ab9fe61db64195b983a20e2e00e0cc19e09423232a88a9cac681e13fc38fcd19). All three profiling/MAILBOX/JIT-name flags are absent. The normal native Vulkan launch entered Green Forest through headless pointer input and captured a 2712x1220 gameplay frame showing 62 FPS (vk-final-gameplay.png). This is one early gameplay frame, not a sustained FPS benchmark or validation of every effect. The runtime remains in the background on Vulkan. Targeted git diff --check passed in both repositories.

References: [Unity rendering threading arguments](https://docs.unity.cn/2021.3/Documentation/Manual/PlayerCommandLineArguments.html), [Android HardwareBuffer usage flags](https://developer.android.com/reference/android/hardware/HardwareBuffer).
