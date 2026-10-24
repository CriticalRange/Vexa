# Portal shader compilation audit — 2026-10-24

Scope: compiler/linker rejection in the visibly incomplete first room. Audio,
camera input and FPS optimization are outside this audit. Existing graphics
probe passes are not a complete Portal rendering pass.

On Xiaomi 2407FPN8EG, the instrumented matched GL host payload records:

- 621 vertex and 1,737 fragment compile operations, all GL_COMPILE_STATUS=1.
- 79 program link operations, all GL_LINK_STATUS=1.
- No failed program-binary loads recorded in this run.
- One captured frame has 276 draw calls and no nonzero pending GL error.
- Existing failed_*.glsl files have older timestamps; no new failed-source dump.

Evidence: scratch/portal/compile-audit/summary.json, audit-complete.log,
map-command-confirmed.png, audit-current.png and shaders/. The ToGL
lightmappedgeneric shader pair was inspected after translation. Its sampling,
lightmap multiplication and fog calculations remain present; shader-input and
texture correctness have not been established by successful compilation.

Conclusion: the tested startup/first-room permutations are accepted by the
compiler and linker. This does not establish semantic translation correctness,
correct textures/uniforms/state, or a Mali driver bug. Large dark areas, slow
rendering, missing audio and camera rotation remain unresolved. The earlier
visible first-room screenshots must not be presented as a fully solved game.

Instrumentation is opt-in through files/vexa_shader_audit.flag. It logs compile,
link and binary-load status without enabling per-frame verbose logging. The
flag and temporary shader dump/input trace flags were removed after capture.
Normal custom-program launch arguments are restored; the verification script
was temporary and is removed afterward. No forced HDR/color disabling is added.
