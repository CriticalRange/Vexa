# PROJECT OVERVIEW

## Assumptions

- Assumption: VEXA's first shipping target is Android on ARM64 devices.
- Assumption: the first runtime path is "run the existing PC/Linux game build through a compatibility layer", not "port the game code to Android".
- Assumption: Hytale remains the first target and the current rewrite focuses on bringing up a debuggable prototype, not a polished consumer app.

## What VEXA Is

VEXA is an Android compatibility runtime and runner for selected PC games.

It combines:

- a simple Android app shell
- runtime preparation and validation
- a native compatibility/runtime layer
- strong diagnostics for startup, crash, and environment failures

The current goal is not broad platform coverage. The current goal is to bring up one target cleanly and understand every failure mode.

## What VEXA Is Not

VEXA is not:

- a traditional console emulator
- a clean-room reimplementation of the target game
- a flashy launcher product
- an optimization-first project
- an AI-designed architecture exercise

It is closer in spirit to Proton-style compatibility work than to a classic emulator or a native mobile port.

## First Target

The first target is Hytale.

That means the rewrite should prioritize:

- Hytale-specific startup requirements
- Hytale-specific runtime diagnostics
- preserving proven findings from HyMobile
- avoiding premature abstractions for future games

## Long-Term Vision

Long term, VEXA should become a small, understandable compatibility runtime framework for bringing specific PC games to Android one at a time.

The intended end state is:

- boring Android UI
- explicit launch/config controls
- live logs and useful crash output
- minimal hidden behavior
- reusable operational knowledge for future targets

Future multi-game support should come only after Hytale startup, launch flow, and crash diagnosis are stable and explainable.

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
