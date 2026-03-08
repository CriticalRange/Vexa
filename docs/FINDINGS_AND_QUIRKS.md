# FINDINGS AND QUIRKS

## Assumptions

- Assumption: this file is a living evidence log, not a final truth document.
- Assumption: items are grouped by confidence so proven findings do not get mixed with guesses.

## Proven Findings

- The project is better modeled as a compatibility runtime than as a native mobile port.
- A usable rootfs/runtime environment is required for the current x86_64-on-Android path.
- External storage is not a safe place to execute everything directly. Internal prepared copies and validation matter.
- Prepared internal copies are required because Android external storage paths are not reliable execution locations for this runtime path.
- Startup is easier to debug when preflight validates files before runtime launch.
- Live logs inside the app are worth keeping. They reduce turnaround time during bring-up.
- Hardcoded package paths are fragile and break renames, flavors, and alternate package names.
- Rootfs readiness is not just "directory exists"; linker and core glibc files must be validated explicitly.
- Missing or stale thunk/runtime support files can produce confusing startup failures.
- Thunk payload presence and freshness affect startup and can fail in confusing ways if not checked early.
- Device-specific behavior matters. MIUI/HyperOS can inject instability that looks unrelated to the target game.
- OpenGL ES capability checks are a real prerequisite on the Android side.
- Package-name-derived internal paths must not be hardcoded into runtime behavior.
- Some FEX-facing runtime areas are signal-sensitive enough that small structural mistakes can cause host-side crashes or deadlocks.
- Upstream FEX has allocator-sensitive areas; convenience abstractions in the wrong place can create correctness risks.
- Upstream FEX is strongly Linux/rootfs/thunk oriented; VEXA should assume an Android-specific integration layer is needed even if deep FEX changes are avoided.
- FEXCore alone is unlikely to be sufficient for VEXA bring-up; rootfs pathing, ELF loading, syscall emulation, and thunk handling appear to live outside the pure core.
- A thin layer around a pinned upstream FEX branch is a safer maintenance strategy than either a deep fork or a pure app-side wrapper around stock Linux binaries.

## Suspected Findings

- Some crashes happen in host/runtime code before the guest can exit cleanly.
- Dirty upgrades can preserve stale runtime payloads and create false regressions.
- Startup races around service/process/thread lifecycle can look like game failures even when the game is not the root cause.
- Flattened or partial rootfs extraction is risky and may silently break expected layouts.
- Signal-handling mistakes in the runtime path can turn a debuggable crash into a confusing host crash.

Treat these as active hypotheses until reproduced cleanly in VEXA.

## Android Quirks

- Android storage policy encourages explicit staging rather than "run from wherever the files are".
- Foreground service behavior should be treated as operational infrastructure, not optional polish.
- Surface lifecycle events can arrive at awkward times and should be logged as first-class events.
- Scoped storage and package-specific directories make path assumptions dangerous.
- OEM Android builds can add their own instability. Device model and OS flavor belong in crash artifacts.

## Old Workarounds Worth Preserving

- Preflight validation before launch.
- Internal staging of runtime-critical files.
- Explicit validation of rootfs linker and library directories before runtime start.
- Explicit staging and validation of guest and host thunk payloads.
- Accessible log files stored somewhere easy to inspect during testing.
- A ready-state or launch-contract artifact that captures what the app believed it was launching.
- Device-specific warning notes, especially for MIUI/HyperOS.
- Runtime/environment override support, but only if clearly labeled and traceable.
- Separation between app-side preflight and runtime-side execution.
- Stable logging around early runtime phases before guest execution.

Preserve the intent of these workarounds first. Re-implement them cleanly later.

## Open Questions

- Which current runtime workarounds are truly required for Hytale, and which only papered over older design mistakes?
- What is the minimum rootfs/library set that still reproduces the known bring-up path?
- Which failures should be classified as app-layer problems versus runtime-layer problems?
- Should the runtime live in the same app process, a dedicated process, or a service-owned process for the first VEXA prototype?
- Which crash artifacts are cheap enough to always capture on device?
- Which startup inputs must be user-editable, and which should stay fixed until bring-up stabilizes?
- What is the smallest VEXA-native FEX handoff that proves rootfs, thunk, and launch wiring without inheriting HyMobile structure?
- Which old runtime workarounds are still required once package-path assumptions are removed?
- Which runtime-facing components must remain extremely low-abstraction because of FEX signal or allocator constraints?
- What is the smallest FEX subset from the pinned `FEX-2601` branch that VEXA actually needs for first bring-up?

## Evidence To Carry Forward

When migrating notes from HyMobile, keep a short record for each item:

- what was observed
- where it was observed
- how repeatable it was
- whether it was app-side, runtime-side, or device-side
- what evidence existed
