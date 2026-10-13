#include <SDL.h>
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

static int fd_count(void) {
  DIR *d = opendir("/proc/self/fd");
  assert(d);
  int n = 0;
  while (readdir(d)) ++n;
  closedir(d);
  return n;
}

int main(void) {
  const Uint32 flags = SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK |
                       SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC;
  assert(flags == 0x3221); /* The flags used by the inspected Unity player. */
  /* Isolate haptic rollback in the negative control: SDL 2.0.22 can crash
     during joystick cleanup after a failed UDEV init with the full flags.
     The positive test always exercises Unity's full subsystem set. */
  const Uint32 tested_flags = getenv("VEXA_TEST_FAIL_MONITOR")
    ? SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_HAPTIC : flags;
  const int before = fd_count();
  for (int i = 0; i < 50; ++i) {
    int result = SDL_Init(tested_flags);
    if (getenv("VEXA_TEST_FAIL_MONITOR")) {
      fprintf(stderr, "negative control: SDL_Init=%d error=%s video=%u\n",
              result, SDL_GetError(), SDL_WasInit(SDL_INIT_VIDEO));
      assert(result < 0);
      assert(SDL_WasInit(SDL_INIT_VIDEO) == 0);
      SDL_Quit();
      puts("PASS: NULL monitor reproduces SDL video rollback");
      return 0;
    }
    if (result < 0) fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    assert(result == 0);
    assert(SDL_WasInit(SDL_INIT_VIDEO) == SDL_INIT_VIDEO);
    assert(SDL_GetNumVideoDisplays() > 0);
    assert(SDL_GetNumDisplayModes(0) > 0);
    SDL_DisplayMode desktop;
    assert(SDL_GetDesktopDisplayMode(0, &desktop) == 0);
    assert(desktop.w > 0 && desktop.h > 0);
    SDL_PumpEvents();
    SDL_Quit();
    assert(SDL_WasInit(0) == 0);
  }
  assert(fd_count() == before);
  puts("PASS: 50 SDL startup/shutdown cycles retain displays and leak no fds");
}
