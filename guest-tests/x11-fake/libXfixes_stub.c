/* Reporting the extension as absent keeps SDL on the core X path. */
const char vexa_sdl_stub[] = "VEXA";

void *XFixesCreatePointerBarrier(void) { return 0; }

void *XFixesDestroyPointerBarrier(void) { return 0; }

void *XFixesQueryVersion(void) { return 0; }
