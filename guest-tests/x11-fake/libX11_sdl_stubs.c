/* Extra Xlib entry points SDL resolves while opening its window. */
#include <stdlib.h>

const char vexa_sdl_stub[] = "VEXA";

/* XWMHints is 56 bytes on LP64. Callers write flags, then input at offset 8.
   A null return is a 32-bit store at address 0x8. */
void *XAllocWMHints(void) { return calloc(1, 64); }

void *XCreateFontSet(void) { return 0; }

void *XCreateImage(void) { return 0; }

void *XDrawRectangle(void) { return 0; }

void *XDrawString(void) { return 0; }

void *XFreeFontSet(void) { return 0; }

void *XFreeStringList(void) { return 0; }

void *XGetDefault(void) { return 0; }

void *XGetErrorText(void) { return 0; }



void *XGetKeyboardControl(void) { return 0; }

void *XGetWMHints(void) { return calloc(1, 64); }

void *XGetWMNormalHints(void) { return 0; }

void *XGrabKeyboard(void) { return 0; }

void *XGrabServer(void) { return 0; }



void *XInstallColormap(void) { return 0; }

void *XKeysymToString(void) { return 0; }

void *XListPixmapFormats(void) { return 0; }

void *XPutImage(void) { return 0; }





void *XRefreshKeyboardMapping(void) { return 0; }

void *XReparentWindow(void) { return 0; }

void *XResetScreenSaver(void) { return 0; }

void *XScreenNumberOfScreen(void) { return 0; }

void *XSetForeground(void) { return 0; }



void *XSetTextProperty(void) { return 0; }

void *XSetTransientForHint(void) { return 0; }

void *XSetWMHints(void) { return 0; }

void *XSetWindowBackground(void) { return 0; }

void *XStoreColors(void) { return 0; }

void *XSupportsLocale(void) { return 0; }

void *XTextExtents(void) { return 0; }

void *XUngrabKeyboard(void) { return 0; }

void *XUngrabServer(void) { return 0; }

void *XUninstallColormap(void) { return 0; }

void *XVisualIDFromVisual(void) { return 0; }



void *XkbGetMap(void) { return 0; }

void *XkbGetUpdatedMap(void) { return 0; }

void *XkbQueryExtension(void) { return 0; }

void *XkbSelectEvents(void) { return 0; }

void *XmbTextListToTextProperty(void) { return 0; }

void *Xutf8DrawString(void) { return 0; }

void *Xutf8ResetIC(void) { return 0; }

void *Xutf8TextExtents(void) { return 0; }

void *Xutf8TextListToTextProperty(void) { return 0; }

/* SDL drops the whole X11 driver if any BASEXLIB symbol is missing. */
static const char vexa_display_string[] = ":0";

void *XChangePointerControl(void) { return 0; }

void *XGetErrorDatabaseText(void) { return 0; }

void *XGetPointerControl(void) { return 0; }

void *XMissingExtension(void) { return 0; }

void *XUnloadFont(void) { return 0; }

const char *XDisplayString(void) { return vexa_display_string; }

void *_XEatData(void) { return 0; }

void *_XFlush(void) { return 0; }

void *_XFlushGCCache(void) { return 0; }

void *_XRead(void) { return 0; }

void *_XReadPad(void) { return 0; }

void *_XSend(void) { return 0; }

void *_XReply(void) { return 0; }

void *_XSetLastRequestRead(void) { return 0; }

void *XSynchronize(void) { return 0; }

void *XESetWireToEvent(void) { return 0; }

void *XESetEventToWire(void) { return 0; }

void *XkbFreeClientMap(void) { return 0; }

void *XAutoRepeatOff(void) { return 0; }

void *XF86VidModeGetViewPort(void) { return 0; }

void *XIBarrierReleasePointer(void) { return 0; }

void *XIGetSelectedEvents(void) { return 0; }

void *XRRConfigCurrentConfiguration(void) { return 0; }

void *XRRConfigCurrentRate(void) { return 0; }

void *XRRConfigRates(void) { return 0; }

void *XRRConfigSizes(void) { return 0; }

void *XRRFreeScreenConfigInfo(void) { return 0; }

void *XRRGetCrtcTransform(void) { return 0; }

void *XRRGetScreenInfo(void) { return 0; }

void *XRRGetScreenSizeRange(void) { return 0; }

void *XRRQueryOutputProperty(void) { return 0; }

void *XRRSetScreenConfigAndRate(void) { return 0; }

void *XShmCreatePixmap(void) { return 0; }

void *XdbeFreeVisualInfo(void) { return 0; }

void *XdbeGetBackBufferAttributes(void) { return 0; }

void *XdbeGetVisualInfo(void) { return 0; }
