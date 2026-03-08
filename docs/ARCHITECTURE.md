# ARCHITECTURE

## Assumptions

- Assumption: the Android app will be rewritten in Kotlin.
- Assumption: the native/runtime side may stay native and separate from the app layer even if implementation details change.
- Assumption: the first runtime target remains an x86_64 Linux game payload launched on Android ARM64 through a compatibility layer.

## High-Level Architecture

VEXA should be treated as two cooperating systems:

1. Android app layer
2. Runtime / compatibility layer

The Android app layer is responsible for control, validation, visibility, and lifecycle coordination.

The runtime / compatibility layer is responsible for actually hosting and executing the target game's runtime environment.

### Suggested Mental Model

- Kotlin app: "operator console"
- Native/runtime layer: "execution engine"
- Prepared files/rootfs/config: "handoff contract"

## Android App Layer Responsibilities

The Android app should own:

- storage selection and file discovery
- startup configuration UI
- preflight validation
- launch request creation
- live log display
- crash summary display
- foreground service and Android lifecycle coordination
- permissions and device-specific warnings
- export/copy/share of logs and crash artifacts
- explicit startup state presentation

The Android app should not own guest runtime semantics.

## Runtime / Compatibility Layer Responsibilities

The runtime layer should own:

- loading the guest executable
- rootfs/runtime environment setup
- validating the prepared rootfs linker and library search paths
- environment variable and argument application
- thunk or bridge library setup
- validating and staging thunk payloads and guest compatibility overlays
- guest process/thread execution
- guest input/display bridging
- signal, exception, and fatal-path capture
- generation of runtime-side logs and crash artifacts
- exposing explicit startup phase markers before guest execution begins
- reporting clear stop/fail status back to the app

The runtime layer should not own Android UI decisions.

## What Belongs In The Android App

- Kotlin activities/fragments/compose views
- launch profiles and config editing
- path validation and missing-file reporting
- "ready/not ready" checks
- log viewer
- crash viewer
- clean-install/dirty-upgrade detection
- device quirk warnings
- user-triggered actions like retry, export logs, clear cache

## What Belongs In The Native / Runtime Layer

- guest ABI/runtime hosting
- rootfs and loader handling
- syscall or platform compatibility logic
- thunk integration
- native surface/input bridge implementation
- signal/thread/memory-sensitive logic
- fatal crash capture close to the failing layer

## Boundary Rules

- The app passes a launch request. The runtime executes it. The app does not improvise runtime semantics.
- The runtime returns explicit state and failure reasons. It does not rely on the app to guess what happened.
- Cross-layer communication should use explicit data contracts, not ad hoc globals.
- Every hidden fallback should be treated as suspicious until documented.
- If a behavior is Hytale-specific, mark it as such instead of pretending it is generic.
- HyMobile is an evidence source, not an implementation base. Reuse proven behaviors, not old structure.
- The new runtime integration should be written from scratch around a small explicit launch contract.
- Do not copy old wrapper object models, globals, or package-specific path assumptions into VEXA.
- Runtime path handling must not depend on hardcoded package names or rename-sensitive internal storage paths.
- Rootfs validation, thunk staging, and runtime artifact paths should be explicit contracts between app and runtime.
- Signal handling behavior in the FEX-facing layer is correctness-sensitive. Treat it as a protected design area, not ordinary app glue.
- Allocator-sensitive or signal-sensitive runtime code should stay minimal and conservative even if higher-level code uses more convenient abstractions.
- Do not move signal, threading, or memory-critical behavior into Kotlin just because it is easier to edit.
- Do not move Android UI concerns into native code.

## Ownership Rules

- You own architecture, runtime semantics, lifecycle policy, and critical implementation decisions.
- Kotlin app code owns presentation, orchestration, and operator-facing tooling.
- Native/runtime code owns guest execution and low-level compatibility behavior.
- AI may help document interfaces and review boundaries, but should not author the core runtime model.

## Lifecycle Overview

1. User opens VEXA and sees target status, config, and known warnings.
2. App runs preflight checks against files, rootfs, support libs, and device requirements.
3. App writes or updates a launch contract.
4. App starts the runtime process/service/session and records a startup breadcrumb.
5. Runtime initializes environment, rootfs, bridges, and guest execution.
6. App shows live logs and current startup phase.
7. Runtime either reaches "guest running" or exits with an explicit failure reason.
8. App surfaces success, clean exit, or categorized crash details with artifact paths.

## Lifecycle States

Keep the public state model simple:

- idle
- checking
- ready
- launching
- runtime-starting
- guest-running
- stopping
- failed
- exited

Avoid ambiguous states like "loading" unless there is a precise meaning behind them.
