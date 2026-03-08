# TODO

## App Shell

- [x] Create the `~/vexa` Android project skeleton in Kotlin.
- [x] Build a deliberately simple prototype launcher UI.
- [x] Add `Play`, `Login`, `Logs`, and `Quit` controls to the prototype screen.
- [x] Launch `GameActivity` from the `Play` button.
- [x] Create `GameActivity` with a dedicated `SurfaceView`.
- [x] Wire `surfaceCreated`, `surfaceChanged`, and `surfaceDestroyed` into `RuntimeBridge`.
- [ ] Replace placeholder launcher status text with a real readiness summary.
- [ ] Show current config summary on the root screen.
- [ ] Show latest failure code on the root screen.
- [ ] Replace static loading overlay text with state-driven startup status messages.
- [ ] Decide whether the logs panel remains on the launcher screen or moves to a dedicated debug screen later.
- [ ] Define what the launcher must know about `GameActivity` and what it must not know.
- [ ] Define the surface ownership rule clearly: app owns Android surface lifecycle, runtime consumes the surface only through an explicit bridge.

## Public State Model

- [ ] Define the public startup state model and keep it small.
- [ ] Define the minimum set of lifecycle phases shown in the UI.
- [ ] Decide which state changes come from Kotlin and which come from the native/runtime side.
- [ ] Decide how the last failure code is stored and cleared.
- [ ] Decide which state is safe to persist across app restarts.

## Logging And Crash Visibility

- [ ] Define the log line format.
- [ ] Define stable log categories.
- [ ] Define severity levels.
- [ ] Implement an in-app live log panel backed by state instead of placeholder text.
- [ ] Add scrolling for the log panel.
- [ ] Add a clear "jump to latest fatal" or equivalent quick-inspect action.
- [ ] Define the breadcrumb artifact format for each launch session.
- [ ] Define the crash summary artifact format with stable failure codes.
- [ ] Decide where session artifacts live on device and keep the path stable.
- [ ] Add copy/export actions for logs and crash artifacts.

## HyMobile Evidence Review

- [ ] Inventory HyMobile files with the triage template and tag them `keep`, `archive`, `rewrite`, or `delete`.
- [ ] Record which HyMobile FEX/runtime behaviors are evidence-only keepers versus old structure to discard.
- [ ] Record which HyMobile workarounds are still unexplained and need proof before reuse.
- [ ] Record which package-path assumptions in HyMobile must never return in VEXA.
- [ ] Write a first-pass Hytale prerequisite checklist based on proven findings only.
- [ ] Record the minimum rootfs/library set that is actually required for the first bring-up.
- [ ] Record the minimum thunk payload set that is actually required for the first bring-up.
- [ ] Record device-specific warnings that must be surfaced before launch, especially MIUI/HyperOS.

## Launch Contract And Preflight

- [ ] Decide the first VEXA runtime handoff contract and document every field.
- [ ] Document the minimum scratch-built VEXA runtime handoff that does not inherit HyMobile wrapper architecture.
- [ ] Decide which parts of preflight are app-side only.
- [ ] Decide which parts of validation must also be repeated by the runtime side.
- [ ] Implement a preflight report that validates paths, rootfs presence, support libraries, and device prerequisites.
- [ ] Validate that the guest executable path is explicit and readable.
- [ ] Validate that rootfs linker candidates are present.
- [ ] Validate that rootfs libc/libm paths are present.
- [ ] Validate that thunk/stub payload directories are present.
- [ ] Validate that required guest and host thunk libraries are present.
- [ ] Validate that artifact output directories are writable.
- [ ] Validate clean-install and dirty-upgrade paths separately.
- [ ] Write the launch request artifact deterministically.

## Fake Runtime Before Real Runtime

- [ ] Build a fake runtime stub that emits lifecycle events and test the app's log/crash UI against it.
- [ ] Feed fake runtime events into the public state model.
- [ ] Feed fake runtime logs into the live log panel.
- [ ] Simulate one controlled startup failure and verify the crash summary path.
- [ ] Simulate one controlled clean exit and verify the exit path.
- [ ] Simulate surface attach/detach events without real guest execution.

## Minimal Native Bring-Up

- [ ] Define the first native wrapper boundary from scratch instead of porting HyMobile wrapper code.
- [ ] Keep the first native wrapper narrow: prepared paths, args/env, artifact paths, and phase reporting only.
- [ ] Decide how the runtime receives the Android surface handle.
- [ ] Decide how runtime startup phases flow back to Kotlin.
- [ ] Decide how runtime failure reasons flow back to Kotlin.
- [ ] Decide how runtime stop requests flow from Kotlin to native code.
- [ ] Reproduce one old workaround only after writing down the specific failure it prevents.
- [ ] Reproduce one known HyMobile startup failure in the new logging model.
- [ ] Only after the above, connect the real native/runtime layer.
- [ ] Verify that the first runtime path can start intentionally and fail explicitly.

## FEX-Specific Runtime Checks

- [ ] Write down the FEX constraints that affect VEXA architecture but do not justify copying old HyMobile structure.
- [ ] Keep signal-sensitive FEX-facing code in a protected low-level area.
- [ ] Keep allocator-sensitive FEX-facing code in a protected low-level area.
- [ ] Decide which old thunk workarounds are still required and which were accidental.
- [ ] Decide which rootfs preparation behaviors are true requirements versus old integration leftovers.
- [ ] Verify that no runtime path depends on hardcoded package names.
- [ ] Verify that runtime-critical files execute from valid internal paths rather than external storage.

## Hytale Bring-Up

- [ ] Track the first repeatable Hytale bring-up point in VEXA.
- [ ] Record exactly what files, rootfs contents, thunk payload, and device conditions were required.
- [ ] Record the first categorized Hytale startup failure reproduced in the new stack.
- [ ] Record the first point where the guest is measurably "running" instead of only "starting."

## Guardrails

- [ ] No major runtime implementation before observability is in place.
- [ ] No new abstractions for future games before Hytale bring-up is stable.
- [ ] No hidden fallback without a documented reason.
- [ ] No AI-authored critical-path runtime logic without explicit human approval.
