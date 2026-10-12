/* Unity Linux player launcher shim.
 *
 * Unity's Linux stub normally starts the engine directly. This shim instead
 * dlopen()s UnityPlayer.so, calls PlayerMain itself and appends command line
 * switches. It exists because two of them are required on VEXA:
 *
 *   -force-glcore      Megabonk ships boot.config with gfx-enable-gfx-jobs=1,
 *                      gfx-enable-native-gfx-jobs=1 and gfx-threading-mode=4,
 *                      which puts the renderer in kGfxThreadingModeClientWorkerJobs.
 *                      On this host the job worker never produces output and the
 *                      game renders nothing at all. Forcing the direct mode
 *                      (kGfxThreadingModeDirect) renders correctly.
 *   -force-gfx-direct  same switch, spelled as Unity's own launcher spells it.
 *
 * Build (guest toolchain, x86_64):
 *   clang --target=x86_64-linux-gnu -O1 -static -o direct_launcher direct_launcher.c -ldl
 *
 * Deploy: copy over files/guests/<game>/<Game>.x86_64. Keep the real stub
 * alongside as <Game>.x86_64.vexa-original so it can be restored.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  void* player = dlopen("./UnityPlayer.so", RTLD_NOW | RTLD_GLOBAL);
  if (!player) {
    fprintf(stderr, "%s\n", dlerror());
    return 1;
  }
  int (*main_fn)(int, char**) = dlsym(player, "_Z10PlayerMainiPPc");
  if (!main_fn) {
    fprintf(stderr, "%s\n", dlerror());
    return 2;
  }

  char** args = calloc(argc + 4, sizeof(char*));
  for (int i = 0; i < argc; ++i) {
    args[i] = argv[i];
  }
  args[argc] = "-force-glcore";
  args[argc + 1] = "-force-gfx-direct";
  fprintf(stderr, "[VEXA][LAUNCH] forcing OpenGL core and direct gfx threading\n");
  return main_fn(argc + 2, args);
}