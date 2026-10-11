/* Guest libX11-xcb.so.1. Qt 6 opens the display through libX11
   (XOpenDisplay) and then pulls the xcb connection out with
   XGetXCBConnection; hand it the VEXA shim connection so every later
   xcb_* call lands in libxcb_fake. */
#include <X11/Xlib.h>
#include <xcb/xcb.h>

static xcb_connection_t* g_connection;

xcb_connection_t* XGetXCBConnection(Display* dpy) {
  (void)dpy;
  if (!g_connection) {
    g_connection = xcb_connect(NULL, NULL);
  }
  return g_connection;
}

void XSetEventQueueOwner(Display* dpy, int owner) {
  (void)dpy;
  (void)owner;
}
