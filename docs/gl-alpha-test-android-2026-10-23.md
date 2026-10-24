# Desktop alpha testing on Android — 2026-10-23

Implemented and device-tested through both i386 and x86_64 FEX GL thunk pairs.
This is an opt-in shader compatibility feature: add `VEXA_GL_ALPHA_TEST=1` to
the program's environment. Portal's installed custom-program record enables it.
The launcher now mirrors this setting into the host environment too.

## Drawing behavior

`glAlphaFunc` stores the comparison and clamped reference per GL context.
`glEnable/glDisable(GL_ALPHA_TEST)`, `glIsEnabled`, and the float, integer,
boolean and double alpha-state queries follow that state. Invalid comparison
enums produce `GL_INVALID_ENUM` without changing the previous setting.

Fragment source is instrumented after desktop-to-ESSL rewriting. The original
main, including declarations and early returns, is wrapped; rejected fragments
execute `discard`. Their depth/stencil and color writes therefore do not occur.
Accepted fragments retain their original color and alpha for blending.
Explicit `early_fragment_tests` is removed so it cannot write depth/stencil
before alpha rejection. Modern vec4 outputs use link reflection to select
color output location zero, including arrays and locations assigned at link.
Legacy `gl_FragColor` and `gl_FragData[0]` are recognized too.

Uniform locations belong to each shared program, while enable/function/reference
belong to the current context. Internal uniforms are refreshed before guest
draws; successful relink and program-binary loads invalidate cached locations.
Encoded selector uniform names survive binary serialization. The native FF
helper shader and direct separable fragment-source path are also instrumented.

## Verification

Real device: Xiaomi 2407FPN8EG, MT6989/Mali, Android 16, ADB TSORX4OB45L779WO.
`guest-tests/graphics/alpha_test_probe.c` compiles into ELF32 and ELF64 probes.
Both ran through the installed guest/host thunk pairs with GLES framebuffer
readback, rather than checking source transformation alone:

```
ALPHA_PROBE_PASS bits=32 cases=36
ALPHA_PROBE_PASS bits=64 cases=36
```

Coverage: all eight functions at alpha below/equal/above the reference;
enable/disable; reference clamping and integer conversion; invalid-enum error;
direct `dlsym` entry; scalar double query with overwrite guards; early return;
rejected depth writes with color masking; blending; independent context state
with a shared program; relink; program binary round trip; output location zero
among MRT outputs; output arrays; explicit early fragment tests; GLSL 120 output.
The counter counts pixel assertions; state and error assertions are additional.
Stencil readback, MSAA and the FF/separable helper routes were not independently
exercised by this probe.

All four final binaries were SHA-256 compared across the FEX build, VEXA assets,
APK entries and installed device files. All match. The final APK was installed,
launched, and the i386 probe passed again after installation.

## Portal result and limits

Portal passes the previous required-entry-point failure for `glAlphaFunc`.
It next stops at required `glAttachObjectARB`. Portal rendering/gameplay is
therefore still unverified; passing alpha tests does not establish that the
game runs. No ARB shader-object compatibility implementation was added here.

This is source instrumentation for conventional legacy outputs and ordinary
global vec4 output declarations, not a complete GLSL parser or full desktop GL
compatibility. Macro-generated/complex output declarations and guest collisions
with the internal `vexa_alpha_*` identifiers need additional handling. Opaque
program binaries compiled without this feature cannot be retrofitted: rebuild
such game shader caches with the feature enabled. Per-game opt-in prevents
rewriting unrelated games' shaders during rollout.

## Source and artifacts

FEX sources are in the sibling `../FEX/ThunkLibs/libGL/`: `alpha_test.inc`,
`program_cache_state.inc`, `libGL_Host.cpp`, `libGL_Guest.cpp`,
`libGL_interface.cpp`, and `android_gl_proc_list.h`. Existing Super Meat Boy VBO
aliases and legacy varying changes were retained. VEXA changes are the host-env
control, paired runtime GL assets/STAMP and the C probe.

Evidence: `scratch/portal/alpha-results/` contains architecture-specific logs and
manifests, Portal logs/screenshot, configuration backup and verified hashes.
Pre-change device GL pairs are in `scratch/portal/alpha-backup/thunks.tar`.

Final APK: `app/build/outputs/apk/debug/app-debug.apk`.
Local build command:

```
gradlew.bat :app:assembleDebug -x verifyFexCompat -x configureFexAndroid -x stageSDL3Thunks
```

The existing FEX checkout does not contain the pinned commit in fex.lock.json;
the ordinary Gradle compatibility check fails before building. For this local
test the already-configured FEX tree was built directly with Ninja, and that
check/configuration was skipped explicitly. The final packaging also skipped
SDL staging after preserving the original unrelated SDL assets. This does not
claim the repository's pinned-FEX build check passes.
