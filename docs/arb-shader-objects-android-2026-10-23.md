# ARB shader-object entry points on Android — 2026-10-23

Implemented `glAttachObjectARB` and the related shader-object API through both
FEX GL thunk ABIs. The new APK is installed on the connected Xiaomi
2407FPN8EG / MT6989 Mali / Android 16 device (ADB TSORX4OB45L779WO).

## Implementation

The sibling FEX tree contains two new files in `ThunkLibs/libGL/`:

- `arb_shader_objects.h`: 36 typed core-forwarding entry points and five generic
  ARB object operations. Linux GLhandleARB is GLuint on both tested guest ABIs.
- `arb_shader_objects_host.inc`: the forwarding implementations and object-type
  dispatch. Core addresses are resolved once per entry point. Forwarding uses
  the existing VEXA core wrappers, retaining source translation, alpha-test
  instrumentation, uniform handling and program-cache lifetime tracking.

`android_gl_proc_list.h` declares the signatures; `libGL_interface.cpp` generates
paired ABI packers and public guest exports, and `libGL_Host.cpp` exposes the
host handlers through the proc resolver. Existing `glShaderSourceARB` continues
to use the pointer-width-aware source bridge. Both proc-address lookup and
direct ELF symbol calls are exercised by the probe.

`glDeleteObjectARB` distinguishes shader and program objects, and zero is
ignored. `glGetObjectParameter{if}vARB` supports object type, subtype, delete,
compile/link/validate status, log/source lengths, attachment counts and uniform
metadata. Wrong-kind queries return INVALID_OPERATION, unknown pnames return
INVALID_ENUM, and invalid object names return INVALID_VALUE, preserving output
parameters. `glGetInfoLogARB` selects the correct shader or program log.
`glGetHandleARB` queries the current GLSL program, validating its enum.

The alpha-specific queued-error field was renamed CompatError, since ARB
validation now uses the same per-context GL error path.

These distinctions follow the [Khronos ARB_shader_objects specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_shader_objects.txt).

## Device verification

`guest-tests/graphics/arb_shader_objects_probe.c` reuses the existing alpha
probe's GLX/FBO harness. Both ELF32 and ELF64 binaries passed:

```
ALPHA_PROBE_PASS bits=32 cases=36
ARB_PROBE_PASS bits=32 pixels=3
ALPHA_PROBE_PASS bits=64 cases=36
ARB_PROBE_PASS bits=64 pixels=3
```

The three additional framebuffer assertions verify ARB-created/linked programs
reject and accept fragments correctly through alpha testing, and remain
drawable after deferred object deletion. Additional state/error assertions cover:

- Mixed core/ARB object creation, explicit source lengths and i386 source arrays.
- Shader compilation, program attachment/link/validation, duplicate attachment.
- Attribute binding/reflection; active uniforms; vector, integer and matrix
  uniform writes, and float/integer uniform readback.
- Generic integer/float object queries, attached object lists with buffer guards,
  shader-source retrieval, current handle and invalid-handle/pname/type errors.
- Compiler information logs, negative log size and unchanged error outputs.
- Deleting attached shaders/current programs, unbinding the last reference,
  and detaching a shader already marked for deletion.

Both host/guest pairs built successfully. The APK was built, installed, and the
i386 probe passed again through the installed package. SHA-256 comparisons
confirm all four GL binaries match across FEX outputs, VEXA assets, APK entries
and installed device files. `git diff --check` passes for the source changes.

## Portal result and remaining scope

Portal advances beyond `glAttachObjectARB`. The next exact startup error is:

```
Could not find required OpenGL entry point 'glBindProgramARB'!
```

That function belongs to the separate legacy ARB assembly-program API, described
in [ARB_vertex_program](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_vertex_program.txt)
and [ARB_fragment_program](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_fragment_program.txt).
It is not an alias for GLSL `glUseProgram`. This change does not implement its
program namespace, assembly upload/translation or execution. Portal still exits
during required-function loading; game rendering and gameplay remain unverified.

The API adapters inherit existing core-thunk/GLES limitations, including the
fixed-function fallback for missing shader stages and desktop matrix transpose
semantics. Shader-source queries currently expose the existing core path's
translated source. No new GL extension is advertised as fully conformant.

## Artifacts and local build

- Source probe: `guest-tests/graphics/arb_shader_objects_probe.c`.
- Logs/manifests/hashes: `scratch/portal/arb-results/`.
- Pre-change GL backup: `scratch/portal/arb-results/thunks-before-arb.tar`.
- APK: `app/build/outputs/apk/debug/app-debug.apk`.

Host32/64 and Guest/Guest_32 GL targets were built directly with Ninja in the
existing `../FEX/build-android-arm64-ninja` configuration. Only the four GL
assets and runtime stamp were staged; unrelated SDL assets were preserved.
Packaging command:

```
gradlew.bat :app:assembleDebug -x verifyFexCompat -x configureFexAndroid -x stageSDL3Thunks
```

The pre-existing FEX checkout/pinned-commit mismatch still blocks the ordinary
compatibility check. The local test build explicitly skips that check and
reconfiguration while packaging the verified local FEX outputs. It does not
establish that the pinned-FEX build check passes.
