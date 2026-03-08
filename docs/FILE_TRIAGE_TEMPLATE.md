# FILE TRIAGE TEMPLATE

Use this template when reviewing files or components from the old project.

The goal is to decide whether each item should be:

- `keep`
- `archive`
- `rewrite`
- `delete`

## Triage Record

### Item

- Path:
- Component name:
- Current role:
- Language/layer:
- Reviewer:
- Date:

### Classification

- Decision: `keep | archive | rewrite | delete`
- Confidence: `high | medium | low`

### Why This Item Exists

- What problem was it originally solving?
- Is that problem still real in VEXA?
- Is the problem app-side, runtime-side, or target-specific?

### Evidence

- Proven behavior:
- Known failures:
- Logs or artifacts:
- Device or platform notes:

### Boundary Check

- Does this belong in the Android app layer?
- Does this belong in the native/runtime layer?
- Is it mixing responsibilities that should now be split?

### Rewrite Value Check

- Is the behavior worth preserving?
- Is the current implementation understandable?
- Is it tightly coupled to old architecture mistakes?
- Would a clean rewrite be cheaper than carrying it forward?

### Risk Check

- Could deleting this remove a proven workaround?
- Could keeping this import hidden assumptions?
- Could rewriting this break a fragile but necessary path?

### Questions To Ask For Every File

- Is this file solving a real current problem or a historical one?
- Do we understand why it exists?
- Has it ever been proven necessary with logs or reproduction?
- Does it hide fallback behavior?
- Does it contain hardcoded paths, package names, or device assumptions?
- Does it belong in Kotlin app code or native/runtime code?
- If we dropped it today, what specific behavior would we lose?

## Decision Guidance

### Keep

Use `keep` when:

- the behavior is proven necessary
- the file's ownership is correct
- the implementation is understandable enough to carry forward for now

### Archive

Use `archive` when:

- the file contains useful history or research
- the behavior is not part of the first VEXA prototype
- deleting it would lose context that may matter later

### Rewrite

Use `rewrite` when:

- the behavior matters
- the implementation is tangled, misleading, or overgrown
- the file mixes too many responsibilities
- the old code would transfer confusion into VEXA

### Delete

Use `delete` when:

- the file solves a dead problem
- the behavior is unproven and unused
- it exists only because of old architectural drift
- it adds noise without carrying important evidence

## Short One-Line Summary

Add one blunt summary at the end:

- Summary:

Example:

- Summary: `Keep the idea, rewrite the implementation.`
