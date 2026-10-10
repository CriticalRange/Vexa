/* Guest libXinerama.so.1. One Android surface, so Xinerama is not available. */
#include <X11/Xlib.h>

Bool XineramaQueryExtension(Display* dpy, int* event_base, int* error_base) {
  (void)dpy;
  if (event_base) {
    *event_base = 0;
  }
  if (error_base) {
    *error_base = 0;
  }
  return False;
}

void* XineramaQueryScreens(Display* dpy, int* number) {
  (void)dpy;
  if (number) {
    *number = 0;
  }
  return NULL;
}
