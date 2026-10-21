# GL clone binary cache — first isolated performance step

The adjacent FEX checkout now caches the linked binary of stripped-output vertex
clones, keyed by the complete transformed source. Repeated sources load a new GL
program from the binary instead of compiling and linking the source again.
Uniform state and original-to-clone location mapping remain independent. The
existing compile path is used on a miss or rejected binary. This does not remove
the first compilation of a unique source, reflection, or uniform fan-out.

The cache owns only bytes, with an 8 MiB payload / 64 entry LRU limit per thread.
It does not retain GL objects across context destruction and is not persisted
between launches. `VEXA_GL_CLONE_CACHE=0` disables lookup, extraction and the
retrievable-binary hint for a controlled baseline. Existing PIPE logs now include
`binary_hit=0/1`. Vulkan and texture conversion were not changed.

Validation on Xiaomi 2407FPN8EG / Mali:

- Both GL-host-64 and GL-host-32 built successfully.
- `guest-tests/graphics/test_clone_binary_cache.py` builds a native Android probe
  using the actual clone implementation. The device passed cache-hit, independent
  red/green rendered uniform values, changed-source miss, rejected-binary fallback,
  LRU eviction and payload-budget checks.
- A simple shader microbenchmark varied: binary/source averaged 0.722/0.751 ms
  on the first run, 0.652/1.058 ms on a later run. This is neither a gameplay
  benchmark nor evidence of a sustained FPS/p99 improvement. Driver source caches
  already reduce compilation cost. Cache extraction adds work on the first miss;
  real-game benefit depends on source repetition and hit rate.
- `:app:assembleDebug` passed; `adb install -r` returned Success. Launching the
  installed app refreshed its runtime stamp to `vexa-graphics-1:b477bc2d6ebb89d8`.
- Installed GL host hashes match packaged assets:
  - 64: `a22d07179e7fa193049a34b831321119ce2a463f2bd2fc32ef2048d54983574f`
  - 32: `ffc3d41a57be652fc4984d24c96d35b7525a5490b4873c59f545b89d6f1f4045`
- Runtime manifest reported `installedMismatches=0`; GL guest and Vulkan host
  payload hashes matched the pre-install device versions.

Build and probe evidence: `logs/clone-cache-20261021/`. Gameplay FPS and first-use
versus repeated-use frame timing still require a matched scene comparison before
calling this a proven performance win.
