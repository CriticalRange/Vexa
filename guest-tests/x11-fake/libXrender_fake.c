/* Guest libXrender.so.1. A null picture format means the window stays opaque. */
#include <X11/Xlib.h>

void* XRenderFindVisualFormat(Display* dpy, Visual* visual) {
  (void)dpy;
  (void)visual;
  return NULL;
}
