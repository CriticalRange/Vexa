/* Reporting the extension as absent keeps SDL on the core X path. */
const char vexa_sdl_stub[] = "VEXA";

void *XF86VidModeGetAllModeLines(void) { return 0; }

void *XF86VidModeGetModeLine(void) { return 0; }

void *XF86VidModeLockModeSwitch(void) { return 0; }

void *XF86VidModeQueryExtension(void) { return 0; }

void *XF86VidModeQueryVersion(void) { return 0; }

void *XF86VidModeSwitchToMode(void) { return 0; }
