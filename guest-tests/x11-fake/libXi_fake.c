/* Guest libXi.so.6. XInput2 is reported missing so Godot stays on core X events. */
#include <X11/Xlib.h>

Status XIQueryVersion(Display* dpy, int* major, int* minor) {
  (void)dpy;
  if (major) {
    *major = 2;
  }
  if (minor) {
    *minor = 0;
  }
  return Success;
}

int XISelectEvents(Display* dpy, Window window, void* masks, int num_masks) {
  (void)dpy;
  (void)window;
  (void)masks;
  (void)num_masks;
  return BadRequest;
}

void* XIQueryDevice(Display* dpy, int deviceid, int* ndevices) {
  (void)dpy;
  (void)deviceid;
  if (ndevices) {
    *ndevices = 0;
  }
  return NULL;
}

void XIFreeDeviceInfo(void* info) {
  (void)info;
}
