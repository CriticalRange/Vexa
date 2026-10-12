#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char** argv) {
  void* player = dlopen("./UnityPlayer.so", RTLD_NOW | RTLD_GLOBAL);
  if (!player) { fprintf(stderr, "%s\n", dlerror()); return 1; }
  int (*main_fn)(int, char**) = dlsym(player, "_Z10PlayerMainiPPc");
  if (!main_fn) { fprintf(stderr, "%s\n", dlerror()); return 2; }
  char** args = calloc(argc + 4, sizeof(char*));
  for (int i = 0; i < argc; ++i) args[i] = argv[i];
  args[argc] = "-force-glcore";
  args[argc+1] = "-force-gfx-direct";
  args[argc+2] = "-force-gfx-direct";
  fprintf(stderr, "[VEXA][DIAG] forcing clamped GL 3.3 and direct rendering\n");
  return main_fn(argc + 2, args);
}
