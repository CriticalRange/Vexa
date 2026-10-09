# PROJECT OVERVIEW

## Assumptions

- Assumption: VEXA's first shipping target is Android on ARM64 devices.
- Assumption: the first runtime path is "run the existing PC/Linux game build through a compatibility layer", not "port the game code to Android".
- Assumption: VEXA is a general-purpose runner for Linux x86_64 programs on Android ARM64, not a client for one game.

## What VEXA Is

VEXA is a general-purpose Android compatibility runtime. It runs Linux x86_64 programs that are bundled with the app or supplied later.

It combines:

- a simple Android app shell
- runtime preparation and validation
- a native compatibility/runtime layer
- strong diagnostics for startup, crash, and environment failures

The current goal is a small catalog of Linux programs that start from the in-app list and report what they printed.

## What VEXA Is Not

VEXA is not:

- a traditional console emulator
- a clean-room reimplementation of the target game
- a flashy launcher product
- an optimization-first project
- an AI-designed architecture exercise

It is closer in spirit to Proton-style compatibility work than to a classic emulator or a native mobile port.

## Programs

The launcher lists Linux x86_64 programs. The first bundled set is three static VEXA probes (hello, args, pid) and two BusyBox applets (echo, uname). A program is an instance: an executable, arguments, a working directory, and a place for its logs.

Startup requirements that belong to one guest stay on that guest. They are not rules for every program.

## Long-Term Vision

VEXA is a general-purpose runner. New Linux programs are added as instances. The runtime stays one compatibility layer.

The intended end state is:

- boring Android UI
- explicit launch/config controls
- live logs and useful crash output
- minimal hidden behavior
- reusable operational knowledge for future targets

More programs are added as instances. Each one names its executable, arguments, and whether it needs a rootfs library or a thunk.

## Core Principles

- Observability first. If something fails, VEXA should say what failed, where, and with what evidence.
- Preserve findings, not accidents. Keep proven behavior and lessons from the old project, but do not preserve confusing structure just because it exists.
- Small boundaries. Android app logic, runtime/native logic, and target-game specifics should stay clearly separated.
- Fail loudly. Avoid silent fallbacks that hide broken assumptions.
- Boring UI. The UI is a control surface and debug surface, not a product showcase.
- Minimal bring-up path. Get to a clean prototype with logs before adding convenience features.
- Human ownership. Architecture, lifecycle semantics, and critical runtime behavior stay under direct human control.

## Non-Goals For Now

- broad game compatibility
- iOS support
- performance tuning
- aggressive memory optimization
- polished theming or launcher UX
- background update systems
- plugin systems
- large cross-game abstractions
- AI-authored core runtime logic
