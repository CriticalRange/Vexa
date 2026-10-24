# ARB assembly program compatibility — 2026-10-23

Implemented `glBindProgramARB` and the related assembly-program API through both
FEX GL thunk ABIs, including a bounded ARBvp/fp-to-ESSL translator and actual
framebuffer drawing. The updated APK is built and installed on the connected
Xiaomi 2407FPN8EG / MT6989 Mali / Android 16 device.

## Behavior

Assembly program names use a separate share-group namespace from GLSL shader
objects/programs. Generated names are reservations until bound. Binding the
same name to a different target reports INVALID_OPERATION. Name zero selects
the context's default object. Deletion releases the name and resets its
bindings, including bindings in other registered contexts in the share group.

Local parameters belong to each object; environment parameters, enables,
bindings and parser error state belong to each context. Float/double setters
and getters, program-string retrieval, binding/length/format/resource queries,
error position and a guest-width-safe error string are implemented. Malformed
inputs preserve query outputs where required and set GL errors.

The translator handles conventional registers/aliases, scalar PARAM aliases,
TEMP/ADDRESS declarations, vector constants, source swizzles and destination
masks. The supported instruction paths include MOV, ADD/SUB/MUL/MAD, saturation,
dot products, common scalar/vector ALU operations, comparisons, address loads,
derivatives, TEX/TXP/TXB and KIL. Unsupported grammar or opcodes report an upload
error with a source position/message. They are never accepted as empty shaders.

Translated vertex/fragment shaders are compiled by the real GLES driver. A
linked pair is cached per context, keyed by source revisions. Local and env
registers are distinct between vertex and fragment stages. Native uniforms are
reflected to update the active array sizes and sampler units. Cache retention
is bounded to 32 pairs per context. Shared-state locking covers the draw's
internal binding and uniforms. Native program binding is restored after each
guest draw, so a following CURRENT_PROGRAM query still observes the guest's
binding. GLSL programs take precedence over the assembly path.

The existing alpha-test instrumentation also applies to translated fragment
programs. Rejecting a fragment and KIL use native shader discard. Drawing with
missing/failed assembly stages reports INVALID_OPERATION without drawing.

Design references: Khronos [ARB_vertex_program](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_vertex_program.txt)
and [ARB_fragment_program](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_fragment_program.txt).

## Verification

`guest-tests/graphics/arb_assembly_probe.c` uses the existing real GLX/FBO harness
and first runs the earlier alpha and ARB shader-object regressions. Both guest
architectures passed:

```
ALPHA_PROBE_PASS bits=32 cases=36
ARB_PROBE_PASS bits=32 pixels=3
ASSEMBLY_PROBE_PASS bits=32 pixels=13
ALPHA_PROBE_PASS bits=64 cases=36
ARB_PROBE_PASS bits=64 pixels=3
ASSEMBLY_PROBE_PASS bits=64 pixels=13
```

The 13 assembly framebuffer assertions cover:

- Alpha rejection/acceptance of an assembly fragment program.
- Vertex local-register changes moving geometry outside the framebuffer.
- Shared program/local-register state and independent context env/enables.
- GLSL precedence and assembly drawing after returning to program zero.
- Program reupload/link-cache invalidation and saturated ALU output.
- Actual texture sampling and interpolated texture coordinates.
- KIL discard and refusal to draw an unsupported/failed program.

Additional assertions check name reservations, target mismatches, source
length/exact byte retrieval, double/float parameter reads, restored native
binding, parser error strings/positions, invalid query/index/count errors,
deletion and cross-context unbinding. The first failing test run exposed a
missing vertex-array setup in the new context of the probe; the probe was
corrected before these passing results were captured.

Both host and guest pairs built. The debug APK was packaged and installed;
the installed i386 probe was rerun. The four GL payload hashes are checked
across build outputs, staged assets, APK entries and installed files.

## Portal

Portal now passes required lookup of `glBindProgramARB`. The new exact startup
failure is:

```
Could not find required OpenGL entry point 'glClipPlane'!
```

The game still stops during required-function loading, before actual Portal
assembly upload or gameplay. Therefore the probe proves this compatibility
path's tested drawing behavior, not Portal rendering or game-wide compatibility.
`glClipPlane` and its drawing behavior were not added in this change.

The public Valve ToGL source in `scratch/portal/togl-source` shows program-name
allocation and binding cleanup even when GLSL is selected. It is older reference
source, not a verified source match for this Portal binary. No conclusion that
Portal actually draws with ARB assembly is inferred from this source alone.

## Scope and implementation limits

This is a bounded compatibility implementation, not full ARB conformance; no
ARB assembly extension is newly advertised. Limits include 128 local and env
registers per stage, 32 temporaries, 16 vertex attributes, 8 texture coordinates
and sampler units, 4 fragment outputs and 1024 instructions. Resource-counter
queries describe the virtual assembly instruction/register model, not measured
GPU machine instructions or performance.

State matrices, PARAM array/range declarations, unsupported OPTION/opcode forms,
mixed GLSL/assembly stages, legacy immediate-mode/FF helper drawing, complete
fixed-function fallbacks and every precision/edge case remain outside the
validated path. Only the tested 2D texture and selected ALU paths have device
pixel proof; other accepted operation branches need broader conformance tests
before advertising the whole extension.

## Source, build and evidence

New FEX files under sibling `../FEX/ThunkLibs/libGL/`:
`arb_assembly.h`, `arb_assembly_state.h`, `arb_assembly_translate.h`,
`arb_assembly_runtime.inc`, and `arb_assembly_host.inc`. Existing context state,
proc declarations/configuration, GL queries, draw wrappers and context cleanup
were integrated. Public entrypoints use generated ABI packers, including double
parameters and source/error strings for i386.

Evidence: `scratch/portal/assembly-results/` contains per-architecture logs and
manifests, Portal logs, installed-package evidence and verified hashes. Its
`thunks-before-assembly.tar` preserves the previous device GL pairs.
APK: `app/build/outputs/apk/debug/app-debug.apk`.

The already-configured host32/64 and guest32/64 GL targets were built directly
with Ninja, then only the four GL assets and runtime stamp were staged.
Packaging used:

```
gradlew.bat :app:assembleDebug -x verifyFexCompat -x configureFexAndroid -x stageSDL3Thunks
```

The pre-existing FEX pinned-commit mismatch remains. This local build explicitly
skips that check/reconfiguration while using the tested local FEX outputs; it
does not claim the normal pinned-FEX compatibility check passes.
