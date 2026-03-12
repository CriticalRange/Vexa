# DEBUGGING STRATEGY

## Assumptions

- Assumption: debugging and explicit crash diagnosis are the top priority for the rewrite.
- Assumption: the app should expose logs directly instead of requiring `adb` for basic triage.

## Logging Philosophy

- Logs are a product feature, not a developer afterthought.
- Every startup phase should emit a clear start and end marker.
- Every fatal path should produce a short human-readable reason and a machine-usable code.
- Avoid vague lines like "failed to initialize" without context.
- Prefer structured log messages over prose.
- Do not silently swallow runtime errors just to keep the UI smooth.
- All Android logcat output should use the single tag `VEXA` so `adb logcat -s VEXA` remains the one stable filter during bring-up.
- The app logger should fan out to both the in-app log store and Android logcat instead of treating them as separate systems.

## Log Categories

Use stable categories so logs can be filtered in the UI:

- `APP`
- `LIFECYCLE`
- `CONFIG`
- `PREFLIGHT`
- `STORAGE`
- `LAUNCH`
- `RUNTIME`
- `NATIVE`
- `GRAPHICS`
- `INPUT`
- `GUEST`
- `CRASH`
- `DEVICE`

Each log line should carry at least:

- timestamp
- category
- severity
- session id
- short message

Recommended Android logcat rendering:

- tag: `VEXA`
- body: `[CATEGORY] message key=value key=value`

## Crash Breadcrumb Idea

Keep a rolling breadcrumb trail for startup and shutdown:

- app opened
- files scanned
- preflight started
- preflight passed or failed
- launch contract written
- runtime start requested
- runtime initialized
- surface attached
- guest started
- guest exited or crashed

Store this as a small append-only artifact per launch session. Keep it lightweight so it survives even when the main runtime fails early.

## Live Log UI Concept

The UI should stay boring and useful:

- one simple live log panel
- category filter
- severity filter
- copy/export button
- "jump to latest fatal" action
- current startup phase shown above the log stream

Useful extras:

- show session id
- show last explicit failure code
- show artifact paths for crash logs

Avoid fancy terminal imitation. Plain readable text is enough.

## Clean-Install Vs Dirty-Upgrade Test Policy

Both paths matter and should be tested separately.

### Clean Install

Use when validating:

- first-run storage setup
- missing-file reporting
- initial runtime staging
- default launch contract generation

### Dirty Upgrade

Use when validating:

- stale runtime payload handling
- schema/config changes
- cache invalidation
- path migrations

Rule:

- never treat a dirty-upgrade success as proof that clean-install works
- never treat a clean-install success as proof that upgrade handling works

## How To Categorize Crashes

Use the narrowest correct bucket:

- `app-crash`: Kotlin/app process failure before or outside runtime execution
- `preflight-fail`: launch blocked by missing or invalid prerequisites
- `launch-contract-fail`: bad or incomplete launch configuration
- `runtime-start-fail`: runtime could not initialize
- `host-runtime-crash`: native/runtime side crashed
- `guest-clean-exit`: target exited intentionally or with a meaningful exit code
- `guest-crash`: target process crashed after successful handoff
- `device-quirk`: OEM or device-specific failure outside normal expectations
- `unknown`: evidence incomplete

## Toolchain Guardrail

- Keep Android native builds pinned to NDK 29 (`ndkVersion` + toolchain path).
- If CMake/AGP falls back to NDK 28, libc++ may expose C++20 mode but still miss `std::atomic_ref`.
- Symptom to recognize quickly:
  - `error: no member named 'atomic_ref' in namespace 'std'`
- First check when this appears:
  - active NDK path in native compile command
  - `android.ndkVersion` in app Gradle config

## What "Explicit Crash Reason" Means In Practice

An explicit crash reason is not "it crashed".

It means VEXA can produce:

- a stable failure code
- a one-line human summary
- the layer that failed
- the last known lifecycle phase
- evidence paths

Example shape:

- code: `preflight_missing_rootfs_linker`
- summary: `Rootfs linker not found in prepared runtime`
- layer: `PREFLIGHT`
- phase: `checking-rootfs`
- evidence: `/path/to/preflight.json`

If the reason is genuinely unknown, say that directly:

- code: `host-runtime-crash_unknown`
- summary: `Runtime crashed before reason capture completed`

Unknown is acceptable. False certainty is not.

## Minimum Debug Artifacts Per Launch

- session log
- breadcrumb file
- launch contract snapshot
- preflight report
- crash summary file on failure

Optional later:

- tombstone collection
- native backtrace symbolization
- guest stdout/stderr split capture
