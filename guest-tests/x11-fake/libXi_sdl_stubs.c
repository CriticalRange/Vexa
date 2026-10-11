/* Touch grabs are unused. SDL keeps the core pointer path. */
const char vexa_sdl_stub[] = "VEXA";

void *XIGrabTouchBegin(void) { return 0; }

void *XIUngrabTouchBegin(void) { return 0; }
