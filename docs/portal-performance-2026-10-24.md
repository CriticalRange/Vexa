# Portal performance diagnosis, 2026-10-24

The first room is visually accepted by the user with mat_hdr_level=0 and
mat_reducefillrate=0. HDR fixing darkness does not fix throughput.

Current status update: GPU BGRA vertex swizzling was subsequently A/B/A tested
and is enabled in the manually deployed host. The current bottleneck after
that change is measured in the final section below. Initial numbers remain
as historical pre-optimization evidence, not current costs.


## Initial measured live render cost

In the stable portal-visible view, 273 direct draws/frame were recorded.
Rolling p50 frame times were around 148–151 ms (about 6.6–6.8 FPS). This is
one stationary first-room view, not a whole-game benchmark.

Temporary aggregate wall-time instrumentation over 128-frame windows recorded:

- Legacy vertex compatibility scope: approximately 40.4–41.6 ms/frame.
- Buffer readback (nested inside the above scope): approximately 35.1–36.6 ms/frame.
- Readbacks: 11,008/128 = 86 per frame.
- Clip uniform application: approximately 1.45–1.50 ms/frame.
- Alpha uniform application: approximately 0.74–0.79 ms/frame.
- Native EGL swap: approximately 1.7–1.8 ms/frame.

Do not add nested scope times. These are CPU-side wall durations, including
native driver waits, not GPU hardware timer results. At this initial measurement stage, the unaccounted roughly
110 ms was not assigned entirely to FEX or Mali; the follow-up below resolves
most of it to native buffer updates. The important confirmed
compatibility cost is the GPU-buffer -> CPU copy -> BGRA conversion -> new
vertex staging path in legacy_client_runtime.inc. It maps index and vertex
buffers, reads them, and stages converted data during draws. Avoiding repeated
readbacks/conversion while preserving BGRA semantics is a concrete optimization
target; no optimization or measured speedup is claimed here.

An 8-second simpleperf CPU-clock profile collected 6,380 samples, zero lost:
MatQueue0 40.31%, Mali command thread 32.02%, Mali event thread 11.13%, main
Thread-6 10.09%. These are CPU sample shares, not GPU utilization or FPS-loss
percentages. Frame-pointer callchains had 10.94% errors overall (27.14% for
MatQueue0); unknown/JIT samples were not fully symbolized. ReadBuffer allocations,
copies, Mali driver calls and MediaTek GED ioctl paths appear in the sample.

engine_no_focus_sleep was tested at 0 and restored to 50, verified in console.
The stable view remained about 150 ms at 0 versus 148–151 ms after restoration;
there is no supported performance gain from that setting in this test.

ThermalService status was 0; that does not exclude manufacturer power limits.
GPU devfreq files were permission-denied, so GPU saturation/frequency throttling
was not established. Worker affinity allowed CPUs 0–7.

Evidence is in scratch/portal/perf-hdr-off/: native perf.data, symbols.txt,
threads.txt, libraries.txt, render-callgraph.txt, stage-room.log,
focus-off-stage.log, focus-restored-stage.log, query screenshots and maps/status.
Temporary timing code and the device flag were removed afterward. Packaged host
libraries were restored; HDR-off launch remains for the user's visual test.


## Full host-thunk breakdown (follow-up measurement)

Temporary timers were inserted at entry/exit of all 425 generated GL host
unpack functions in both ABIs. Per-thread counters counted only the outermost
wrapper, preventing nested double counting. Counters were printed once per
128 swaps; no per-call tracing, glGetError or GPU completion was added.
The experiment used the same HDR-off first-room portal-visible view, 273
DrawRangeElementsBaseVertex calls/frame. Screenshot: perf-breakdown/stable-scene.png.

Last three complete stable 128-frame windows (384 frames total):

| CPU-side wall duration | Mean ms/frame | Calls/frame |
|---|---:|---:|
| glBufferSubData | 69.257 | 216 |
| glDrawRangeElementsBaseVertex, including compatibility work | 47.629 | 273 |
| All other measured GL wrappers | 10.174 | variable |
| Outside measured GL wrappers on the render thread | 20.481 | - |
| Total frame interval | 147.541 | - |

Individual frame-window averages: 143.998, 149.745, 148.880 ms (about 6.78 FPS
from the combined mean). Host GL total: 127.060 ms/frame, about 86% of the
interval. Upload + draw wrappers account for about 79% of the interval.
The upload wrapper only checks the disabled TBO logging switch then calls
native GLES glBufferSubData; no vertex conversion occurs inside this wrapper.
Thus this new ~69 ms cost is at the native buffer-update boundary, including
any driver CPU work, waits and scheduling, not an ARB shader translation cost.
It does not prove GPU execution alone or identify the exact synchronization cause.

The other GL bucket includes glBindFramebuffer 2.244 ms (39/frame),
glBlitFramebuffer 2.218 ms (7/frame), the complete swap wrapper 1.905 ms
(1/frame), and glUniform4fv 0.798 ms (about 441/frame), plus other state calls.
Earlier separate measurements put legacy array staging at about 40-42 ms,
including 35-37 ms buffer readback, inside the draw bucket. Those earlier
nested figures are not additional time and were not remeasured in these same
384 frames. The remaining draw bucket includes alpha/clip setup, native draws,
state restoration and scheduling; it is not a pure GPU draw timer.

The ~20.5 ms outside GL includes guest rendering code, FEX transitions and
synchronization/scheduling between wrappers. It is not established as pure
FEX translation overhead. Uncovered GL entry routes, if any, would also fall
in this bucket; generated wrappers are the observed game GL dispatch route.

An additional 8-second CPU-clock sample and schedstat snapshot pair were saved.
The snapshot interval was about 8.80 seconds, with sequential ADB snapshot
boundary uncertainty. MatQueue0 accumulated 5.167 seconds scheduled CPU,
0.255 seconds runnable delay, and approximately 3.381 seconds off CPU.
The main game thread accumulated 1.354 seconds CPU and about 7.431 seconds
off CPU. These thread timelines overlap and must not be added to frame time.
Native GED GPU-hint/ioctl stacks remain prominent. This supports substantial
native driver/synchronization work; it does not isolate all waits to uploads.
The new render callgraph had 64% unwind errors, so inclusive stack percentages
must not be interpreted as a complete accounting.

Priority supported by measurement: investigate 216 native buffer updates/frame
(~69 ms), alongside 86 legacy buffer readbacks/frame (~35 ms nested in draws).
A correct optimization must preserve buffer visibility and vertex BGRA semantics;
orphaning/ring buffers, shadow copies or GPU conversion remain experiments until
same-scene A/B validation. No optimization or speedup was implemented here.

Evidence: scratch/portal/perf-breakdown/{allgl-final.log,stable-windows.json,
stable-means.json,sched-delta.json,portal-breakdown.data,render-stable.txt,
symbols-stable.txt,stable-scene.png}. Temporary source and generated-wrapper
instrumentation were restored byte-for-byte afterward. Device hosts restored
from the packaged baseline; no permanent diagnostic instrumentation remains.


## GPU BGRA optimization and current main bottleneck

The implementation keeps supported, buffered vec4 BGRA attributes in the GPU
buffer and performs the component swizzle in the vertex shader. CPU client
arrays, unsupported inputs and uninstrumented shaders retain the earlier
fallback. The A/B/A comparison in the same 273-draw view measured roughly
148-152 ms with CPU staging, 110-116 ms with GPU swizzle, then 146-148 ms when
CPU staging was restored. After disabling the separate mapped-upload trial,
normal GPU-swizzle results were around 107-109 ms. Static wall pixels were
identical in a selected screenshot region; full-scene animated pixels differ.
This is not whole-game correctness/performance proof or a claim that the
extended new regression probes have run. The feature remains in source and
manually deployed host libraries, not a newly packaged APK from this follow-up.
Evidence: scratch/portal/optimization/{bgra-aba-final.log,
stopped-at-bgra.log,image-difference.json,control-scene.png,gpu-scene.png}.

### Fresh CPU-side wall and scheduled CPU measurements

All 425 generated unpack wrappers were temporarily instrumented again, with
top-level nesting exclusion, per-thread wall clocks and CLOCK_THREAD_CPUTIME_ID.
The same stationary portal-visible HDR-off view was used, with GPU BGRA enabled
and both upload experiments disabled. Three stable 128-frame windows (384 frames):

| Disjoint wall-time bucket | Mean ms/frame |
|---|---:|
| glBufferSubData, 216 updates/frame | 67.797 |
| glDrawRangeElementsBaseVertex, 273 draws/frame | 10.264 |
| Other measured GL calls (including swap) | 12.593 |
| Outside measured GL wrappers on render thread | 17.970 |
| Total interval | 108.624 |

Buffer updates alone account for about 62% of the remaining frame interval.
Their measured scheduled thread CPU was 33.677 ms/frame; the approximately
34.120 ms wall-minus-CPU remainder is time not executing on that thread,
including blocked/runnable delay and timing overhead. It is not a hardware GPU
execution measurement, and it does not assign every wait specifically to a
GPU fence. CPU and wall columns overlap; they are not additive frame buckets.
The draw bucket is now about 10 ms instead of the old 48 ms, so the old
35-37 ms readback cost must not be reused as a current residual problem.

Instrumentation adds clock syscalls. In particular, whole-window CPU minus
per-call CPU assigns some clock-accounting overhead to the outside bucket;
that residual can exceed the outside wall duration by a few ms. Do not publish
that residual as precise guest/FEX CPU time. The upload call's CPU/wall split
is useful at the shown approximate scale; no sub-ms accuracy is claimed.
Uninstrumented ~107 ms results support that this profiling has not created the
main ~68 ms upload bottleneck.

A fresh 8-second CPU-clock DWARF sample of the uninstrumented optimized host
captured 2800 samples overall, 1065 on MatQueue0. About 57.84% of render-thread
CPU samples were beneath glBufferSubData. The observed native stacks include
Mali queue/submission code, osup_sync_object_wait, condition/futex calls,
ppoll/write/eventfd and MediaTek ged_gpu_hint_cpu/ged_bridge_call/ioctl.
These are sampled CPU shares, not disjoint wall-time percentages.
Mali's command worker had 924 samples; its CPU execution overlaps the render
thread and cannot be added as sequential frame cost.

A read-only copy of the loaded Mali driver has SHA-256
68d8277a607e8005466337ce9a7c4e33370865b2e6f3ebb38eaa69623f744374.
REA identified it as ARM64 ELF with that digest, but its deep provider was
unavailable on this Windows host. Focused LLVM disassembly (ordinary shell
evidence, not REA decompilation) confirms the glBufferSubData entry at 0x9017c0
routes into 0x8e93a0, whose upload path calls 0x8ec1a0. In the sampled wait path,
0xa2d920 invokes ged_gpu_hint_cpu around 0x1973f40. Most internal symbols are
stripped; semantic names for all intermediate functions are not established.
No driver binary was patched. Exact fence-vs-driver-worker ownership remains
unknown; the combined buffer-update/driver synchronization boundary is measured.

simpleperf --trace-offcpu failed because sched:sched_switch was not available.
Perfetto advertised GPU memory but no GPU-counter/render-stage producer in the
queried inventory. GPU hardware busy/frequency was therefore not established.
The existing glms/f remains trace-gated CPU-side accounting: zero with tracing
off cannot show that the GPU is idle or unsaturated.

### Supplementary resolution check

Requested 640x360 was clamped by Source to 640x480; console.log records
1280x720 -> 640x480 and the later restoration to 1280x720. The low-mode picture
and FOV changed, and the native display remained 2712x1220. Two stable 128-frame
low-mode windows averaged 104.062 ms, 272 draws and 216 updates/frame; upload
wall time was still 65.555 ms (33.025 ms scheduled CPU). This is about a 4%
frame-time reduction and supports that this scene's dominant problem is not
simply guest-resolution-dependent pixel shading. It is not a fully identical
render workload, and does not rule out GPU cost in other passes or scenes.

The concrete bottleneck to address is the interaction of frequent partial
vertex/index buffer writes with the Mali driver's work-queue/resource
synchronization path. An unsafe unsynchronized map is not a valid fix.
The earlier synchronized mapped-upload trial slowed the same heavy view to
about 250 ms and is disabled. Whole-store orphaning did not apply to these
partial writes (zero full-store updates in the measured windows). A future
correct optimization must preserve untouched buffer data and in-flight draw
versions, for example through a validated streaming/versioning strategy.

Evidence: scratch/portal/bottleneck-current/{cpu-wall-summary.json,
720-stable-windows.json,720-before.log,360-stable.log,resolution-console.log,
dwarf-render.txt,dwarf-self.txt,dwarf-driver.txt,dwarf-threads.txt,
portal-current.data,offcpu-record.txt,*.asm}. Temporary all-wrapper timers were
removed and the original generated files restored byte-for-byte. The exact
pre-measurement device hosts were restored; GPU BGRA remains enabled, HDR is
still off, and the game resolution was returned to 1280x720.
