/* Guest libXrandr.so.2. RandR is absent, so the guest keeps the X screen size. */
#include <X11/Xlib.h>

Bool XRRQueryExtension(Display* dpy, int* event_base, int* error_base) {
  (void)dpy;
  if (event_base) {
    *event_base = 0;
  }
  if (error_base) {
    *error_base = 0;
  }
  return False;
}

Status XRRQueryVersion(Display* dpy, int* major, int* minor) {
  (void)dpy;
  if (major) {
    *major = 1;
  }
  if (minor) {
    *minor = 2;
  }
  return 0;
}

void* XRRGetScreenResources(Display* dpy, Window window) {
  (void)dpy;
  (void)window;
  return NULL;
}

void* XRRGetCrtcInfo(Display* dpy, void* resources, XID crtc) {
  (void)dpy;
  (void)resources;
  (void)crtc;
  return NULL;
}

void* XRRSizes(Display* dpy, int screen, int* nsizes) {
  (void)dpy;
  (void)screen;
  if (nsizes) {
    *nsizes = 0;
  }
  return NULL;
}
