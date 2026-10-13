# Vampire Survivors runtime fixes (2026-10-14)

## Loading

The offline Steam flat API returned handle zero for FileWriteAsync and reported every call unfinished. Facepunch CallResult waited indefinitely while SaveSystem created its initial save. The stub now provides persistent local reads/writes and typed completion (callback 1331). Atomic replacement preserves previous saves on failure. The Android native storage regression test passed (STORAGE_TEST_OK), and the actual game created and reopened SaveData.

After storage progressed, Mono preload crashed while FEX speculatively decoded past calls into writable JIT code. The Linux runtime now activates existing FEX Mono compatibility before translation for the explicitly opted-in Vampire Survivors profile (VEXA_MONO_COMPAT=1). The photosensitivity screen and game scene render after this change.

## Input

Fake X11 hardcoded all pointer/key event destinations and reported focus as window 1. Unity creates helper window 1 and selects input on game window 2 (mask 0x62c07f). The bridge now tracks input selection/explicit focus and sends pointer/key events to that window. This also repairs the shared keyboard path used by GuestPad and Android physical controllers.

Runtime evidence: logs/vs-20261014/input/Player.log records input_window=2 and pointer presses; screen.png shows the running level. The debug APK was built and installed successfully. User confirmed GuestPad and physical controller input work. This change does not implement Unity-native evdev controller enumeration; type-7 controller records remain available to external SDL thunks, while this Unity build receives the existing keyboard/mouse mappings.

## Audio

The staged ALSA binary was older than the current bridge source. Updating it resolved intermittent audio (user confirmed). Buffer availability now respects configured capacity and actual reusable OpenAL buffers.

The user subsequently reported music sounding fast. The existing blocking write loop returned EAGAIN after just 20 x 2 ms even with nonblock disabled. Baseline runtime logged repeated EAGAIN while the queue was full. Removing this false 40 ms timeout lets every PCM block wait for host queue space; only a one-second stall reports EPIPE. Vampire Survivors also opts into VEXA_ALSA_BLOCKING=1 for explicit pacing. The bridge honors this option in both open and snd_pcm_nonblock calls.

Final runtime evidence: logs/vs-20261014/audio-paced3.log, 44,100 Hz feed matches elapsed wall time, EAGAIN count zero over approximately 31 seconds. User confirmed normal music tempo. Megabonk was subsequently verified in a running level for approximately 77 seconds: zero EAGAIN, zero Android AudioTrack underruns, no source restart after initial start. User confirmed normal sound. Evidence: logs/megabonk-audio-20261014/. Build/stage with python guest-tests/audio/build_alsa_bridge.py before assembling the APK.
