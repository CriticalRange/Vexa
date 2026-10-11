/* Reporting the extension as absent keeps SDL on the core X path. */
const char vexa_sdl_stub[] = "VEXA";

void *XScreenSaverQueryExtension(void) { return 0; }

void *XScreenSaverQueryVersion(void) { return 0; }

void *XScreenSaverSuspend(void) { return 0; }
