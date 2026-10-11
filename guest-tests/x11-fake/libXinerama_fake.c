/* Guest libXinerama.so.1. One Android surface = one Xinerama screen covering
   the published guest size, so clients see a single active display. */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/* XineramaScreenInfo is defined in the client header; recreate it here so the
   fake needs no Xinerama dev headers at build time. */
typedef struct {
  int screen_number;
  short x_org;
  short y_org;
  short width;
  short height;
} VexaXineramaScreenInfo;

static void vexa_xinerama_size(int* width, int* height) {
  char buf[64];
  int fd = open("/tmp/vexa-window-size", O_RDONLY);
  *width = 1280;
  *height = 720;
  if (fd < 0) {
    return;
  }
  const ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) {
    return;
  }
  buf[n] = 0;
  int w = 0;
  int h = 0;
  if (sscanf(buf, "%d %d", &w, &h) == 2 && w > 1 && h > 1) {
    *width = w;
    *height = h;
  }
}

Bool XineramaQueryExtension(Display* dpy, int* event_base, int* error_base) {
  (void)dpy;
  if (event_base) {
    *event_base = 0;
  }
  if (error_base) {
    *error_base = 0;
  }
  fprintf(stderr, "[VEXA][XINERAMA] query_extension -> active\n");
  return True;
}

Status XineramaQueryVersion(Display* dpy, int* major, int* minor) {
  (void)dpy;
  if (major) {
    *major = 1;
  }
  if (minor) {
    *minor = 1;
  }
  fprintf(stderr, "[VEXA][XINERAMA] query_version\n");
  return 1;
}

Bool XineramaIsActive(Display* dpy) {
  (void)dpy;
  fprintf(stderr, "[VEXA][XINERAMA] is_active -> True\n");
  return True;
}

void* XineramaQueryScreens(Display* dpy, int* number) {
  (void)dpy;
  int width;
  int height;
  vexa_xinerama_size(&width, &height);
  VexaXineramaScreenInfo* info = (VexaXineramaScreenInfo*)calloc(1, sizeof(*info));
  if (!info) {
    if (number) {
      *number = 0;
    }
    return NULL;
  }
  info->screen_number = 0;
  info->x_org = 0;
  info->y_org = 0;
  info->width = (short)width;
  info->height = (short)height;
  if (number) {
    *number = 1;
  }
  fprintf(stderr, "[VEXA][XINERAMA] query_screens -> 1 (%dx%d)\n", width, height);
  return info;
}
