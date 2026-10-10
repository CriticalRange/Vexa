/* Guest libXext.so.6. The shape extension is absent; Godot skips it. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

Bool XShapeQueryExtension(Display* dpy, int* event_base, int* error_base) {
  (void)dpy;
  if (event_base) {
    *event_base = 0;
  }
  if (error_base) {
    *error_base = 0;
  }
  return False;
}

void XShapeCombineRegion(Display* dpy, Window window, int dest_kind, int x_off, int y_off, Region region, int op) {
  (void)dpy;
  (void)window;
  (void)dest_kind;
  (void)x_off;
  (void)y_off;
  (void)region;
  (void)op;
}
