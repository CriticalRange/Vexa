# VEXA

VEXA is an Android compatibility runtime and runner for selected PC games.

The current rewrite is focused on one target, one bring-up path, and strong diagnostics rather than broad compatibility or polished launcher UX. The first target is Hytale.

## What This Repo Is For

This repository currently contains the Android app shell and the early control/debug surface for VEXA:

- a Kotlin Android app
- a launcher activity
- a game activity backed by a `SurfaceView`
- early in-app logging and debug UI
- project documentation for architecture, bring-up, and findings

The goal right now is observability first: make startup, failure, and runtime handoff visible before deeper native/runtime integration.

## Project Direction

VEXA should be treated as two cooperating layers:

- Android app layer: control, validation, lifecycle coordination, logging, crash visibility
- Runtime / compatibility layer: rootfs setup, runtime hosting, guest execution, low-level compatibility behavior

This project is closer in spirit to Proton-style compatibility work than to a traditional emulator or a native Android port.

## Current Status

The Android prototype currently includes:

- launcher-to-game activity flow
- `SurfaceView` lifecycle wiring through `RuntimeBridge`
- loading overlay work
- in-app debug console and log filtering
- fatal-state summary on the launcher screen

The real native/runtime path is still under active bring-up.

## Documents

Project documents live under [docs](/home/critical/vexa/docs):

- [Project Overview](/home/critical/vexa/docs/PROJECT_OVERVIEW.md)
- [Architecture](/home/critical/vexa/docs/ARCHITECTURE.md)
- [Debugging Strategy](/home/critical/vexa/docs/DEBUGGING_STRATEGY.md)
- [Findings And Quirks](/home/critical/vexa/docs/FINDINGS_AND_QUIRKS.md)
- [Rewrite Plan](/home/critical/vexa/docs/REWRITE_PLAN.md)
- [TODO](/home/critical/vexa/docs/TODO.md)
- [AI Usage Policy](/home/critical/vexa/docs/AI_USAGE_POLICY.md)
- [File Triage Template](/home/critical/vexa/docs/FILE_TRIAGE_TEMPLATE.md)

## Principles

- observability before optimization
- explicit failures over hidden fallback
- small boundaries between app and runtime
- preserve proven findings, not old structure
- keep the UI as an operator/debug surface

## Notes

- `AGENTS.md` stays at the repository root and defines AI collaboration constraints for this repo.
- HyMobile is treated as an evidence source, not an implementation base.
