# REWRITE PLAN

## Assumptions

- Assumption: the rewrite target is a new repo rooted at `~/vexa`.
- Assumption: the old HyMobile project is now a source of evidence, not a source of truth.
- Assumption: the first rewrite pass is documentation-first and prototype-first.

## What To Preserve From Old Work

Preserve findings that have already cost time to learn:

- required runtime preparation steps
- rootfs and library layout expectations
- proof that internal staging is required on Android for executable runtime content
- proof that rootfs linker and library validation must happen before runtime start
- known Android storage and execution constraints
- known device quirks, especially MIUI/HyperOS behavior
- useful preflight checks
- proof that thunk payload staging and freshness checks are operationally important
- useful crash signatures and failure patterns
- proven debug affordances like live logs and accessible artifact paths
- proof that hardcoded package paths are a design bug, not a convenience
- known package/runtime path assumptions that failed and should now be avoided

Preserve these as notes, checklists, and explicit tests before preserving them as code.

## What To Throw Away

Throw away:

- architecture that came from treating AI like the lead engineer
- old "mobile port" framing when the actual system is a compatibility runtime
- dead code that never became a stable behavior
- hidden fallbacks that change semantics silently
- the old wrapper object structure and singleton/global layout
- hardcoded package names and rename-sensitive paths
- package-name-derived filesystem assumptions
- any runtime behavior whose only justification is "HyMobile used it"
- broad abstractions for future games before Hytale is stable
- UI complexity that is not required for launch and debug

## What To Rewrite First

Rewrite the pieces that create clarity:

1. documentation and boundaries
2. Kotlin app shell with boring controls
3. log pipeline and crash artifact surface
4. preflight validator and launch contract
5. minimal runtime handoff path
6. only then deeper runtime integration work

This keeps the early rewrite explainable and testable.

## Phased Migration Plan

## Phase 0: Archive And Triage

- copy existing notes, logs, and known quirks into `~/vexa`
- classify old files as keep/archive/rewrite/delete
- record proven behaviors separately from implementation guesses
- define the public VEXA terminology

Exit criteria:

- documentation set exists
- old repo components are triaged
- the rewrite scope is explicit

## Phase 1: App Skeleton

- create the Kotlin Android project
- keep the UI intentionally simple
- add screens for status, config, logs, and crash summary
- define startup states and failure codes

Exit criteria:

- app launches
- app can display static readiness and log views
- no runtime integration yet

## Phase 2: Observability Before Execution

- define log categories and log sinks
- define breadcrumb format
- define crash summary format
- define preflight report format
- make log export and artifact browsing work

Exit criteria:

- app can show live logs from a fake or stub source
- startup and failure states are visible

## Phase 3: Launch Contract And Preflight

- define the launch configuration format
- implement file discovery and validation
- detect missing assets/rootfs/libs early
- distinguish clean install from dirty upgrade cases

Exit criteria:

- app can prove why the target is or is not launchable
- launch request artifact is written deterministically

## Phase 4: Minimal Runtime Bring-Up

- connect the app to the native/runtime layer
- implement a new minimal native wrapper from scratch instead of porting HyMobile wrapper code
- start the runtime with the smallest possible target path
- keep the first runtime handoff intentionally narrow: prepared paths, args/env, artifact paths, and phase reporting
- capture runtime startup phases
- ensure explicit failure output on every early-exit path
- preserve proven workarounds only when they can be justified by evidence or upstream FEX constraints
- treat signal-sensitive and allocator-sensitive FEX-facing code as protected low-level implementation

Exit criteria:

- runtime can be launched intentionally
- failures come back with category and evidence

## Phase 5: Hytale-Specific Bring-Up

- reintroduce only the Hytale-specific requirements that are proven necessary
- validate input, surface, and startup behavior
- track first stable "guest reached running state" milestone

Exit criteria:

- Hytale reaches a repeatable bring-up point
- failures are diagnosable without guessing

## Milestones

- M1: `~/vexa` repo created with baseline docs
- M2: Kotlin app shell boots and shows status/log panels
- M3: log, breadcrumb, and crash-summary pipeline exists
- M4: preflight validator explains missing runtime prerequisites
- M5: launch contract is written and consumed
- M6: runtime can be started and stopped intentionally
- M7: first categorized Hytale startup failure is reproduced in the new stack
- M8: first repeatable Hytale launch progress point is reached

## Rewrite Rules

- one change should answer one question
- if a behavior is not understood, document it before abstracting it
- do not import large old files just to move faster
- do not preserve legacy naming if it hides the new model
- prefer replacing a vague subsystem with a small clear one
