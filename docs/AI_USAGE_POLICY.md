# AI USAGE POLICY

## Purpose

AI is a support tool for VEXA.

AI helps reduce clerical work, improve review quality, and preserve context. AI does not own the project.

## What AI Is Allowed To Do

- write and refine documentation
- create checklists and migration plans
- summarize findings from old files and notes
- review small scoped designs for gaps and risks
- review diffs for regressions and unclear boundaries
- help classify files and components
- draft tiny support scripts or utilities when explicitly requested
- explain concepts, platform behavior, and debugging approaches

## What AI Is Not Allowed To Do

- define the core architecture on its own
- own lifecycle semantics
- own threading, signal, memory, or low-level runtime behavior
- author large runtime files by default
- introduce speculative abstractions for future games
- silently "clean up" critical code paths without a narrow brief
- replace direct technical judgment on high-risk code

## Rules For Accepting AI-Generated Code

- accept only small, narrow diffs unless explicitly requested otherwise
- require a human-written intent before code generation
- review every line before keeping it
- reject any change that hides failure behavior
- reject any change that invents new architecture without approval
- treat AI code as draft material until tested and understood
- prefer code that improves diagnostics, tooling, or clarity over code that touches runtime semantics

## Rules For Documentation Requests

- AI should state assumptions clearly.
- AI should separate proven findings from guesses.
- AI should keep documents practical and repo-usable.
- AI should preserve existing project constraints instead of replacing them.
- AI should prefer checklists, risk notes, and explicit boundaries over broad theory.

## Rules For Review Requests

- findings first
- cite concrete files and lines when possible
- focus on bugs, regressions, hidden assumptions, and scope drift
- say when confidence is low
- do not pad the review with praise
- identify missing tests or missing evidence explicitly

## Human Override Rule

If AI output conflicts with your architecture, your observed behavior, or your debugging evidence, AI loses.

VEXA is human-directed. AI is advisory only.
