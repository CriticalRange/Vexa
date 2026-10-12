/* Guest libXxf86vm.so.1 - XFree86-VidMode extension.
   SDL's X11 backend asks this extension for the list of available video modes
   when the mode list is not coming from XRandR. Returning NULL from every entry
   point makes SDL believe there are no modes at all, and Unity's
   Screen.resolutions then comes back empty, which crashes any game that
   indexes Screen.resolutions[0] while restoring its settings.

   So implement it properly: advertise the extension and hand back the current
   surface size plus a few common sizes, mirroring libXrandr_fake.c. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/* XF86VidModeModeInfo, verbatim from <X11/extensions/xf86vmode.h>. Kept locally
   because the sysroot we build against does not ship the header. */
typedef struct {
  int dotClock;
  short hDisplay;
  short vDisplay;
  short hSyncStart;
  short hSyncEnd;
  short hTotal;
  short hSkew;
  short vSyncStart;
  short vSyncEnd;
  short vTotal;
  unsigned char hSyncPolarity;
  unsigned char vSyncPolarity;
  unsigned char flags;
  unsigned char pad1;
  unsigned short pad2;
} XF86VidModeModeInfo;

static void vexa_vidmode_size(int* width, int* height) {
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

/* The list SDL will walk: current surface size first, then a couple of sizes
   common enough that games which persisted one of them still find it. */
static const int kExtra[3][2] = {{1920, 1080}, {1280, 720}, {1024, 768}};

static void vexa_fill_mode(XF86VidModeModeInfo* m, int w, int h) {
  memset(m, 0, sizeof(*m));
  m->dotClock = (w + 96) * (h + 10) * 60;
  m->hDisplay = (short)w;
  m->vDisplay = (short)h;
  m->hSyncStart = (short)w;
  m->hSyncEnd = (short)(w + 32);
  m->hTotal = (short)(w + 96);
  m->hSkew = 0;
  m->vSyncStart = (short)h;
  m->vSyncEnd = (short)(h + 3);
  m->vTotal = (short)(h + 10);
  m->flags = 0;
}

Bool XF86VidModeQueryExtension(Display* dpy, int* event_base, int* error_base) {
  (void)dpy;
  if (event_base) {
    *event_base = 0;
  }
  if (error_base) {
    *error_base = 0;
  }
  fprintf(stderr, "[VEXA][VIDMODE] query_extension -> present\n");
  return True;
}

Bool XF86VidModeQueryVersion(Display* dpy, int* major, int* minor) {
  (void)dpy;
  if (major) {
    *major = 2;
  }
  if (minor) {
    *minor = 2;
  }
  fprintf(stderr, "[VEXA][VIDMODE] query_version -> 2.2\n");
  return True;
}

Bool XF86VidModeGetModeLine(Display* dpy, int screen, XF86VidModeModeInfo* mode) {
  (void)dpy;
  (void)screen;
  int w;
  int h;
  vexa_vidmode_size(&w, &h);
  if (!mode) {
    return False;
  }
  vexa_fill_mode(mode, w, h);
  fprintf(stderr, "[VEXA][VIDMODE] get_mode_line -> %dx%d\n", w, h);
  return True;
}

XF86VidModeModeInfo** XF86VidModeGetAllModeLines(Display* dpy, int screen, int* count) {
  (void)dpy;
  (void)screen;
  int w;
  int h;
  vexa_vidmode_size(&w, &h);
  const int total = 1 + (int)(sizeof(kExtra) / sizeof(kExtra[0]));
  XF86VidModeModeInfo** lines = (XF86VidModeModeInfo**)calloc((size_t)total, sizeof(*lines));
  if (!lines) {
    if (count) {
      *count = 0;
    }
    return NULL;
  }
  lines[0] = (XF86VidModeModeInfo*)calloc(1, sizeof(**lines));
  vexa_fill_mode(lines[0], w, h);
  for (int i = 0; i < total - 1; ++i) {
    lines[i + 1] = (XF86VidModeModeInfo*)calloc(1, sizeof(**lines));
    vexa_fill_mode(lines[i + 1], kExtra[i][0], kExtra[i][1]);
  }
  if (count) {
    *count = total;
  }
  fprintf(stderr, "[VEXA][VIDMODE] get_all_mode_lines -> %d modes (current %dx%d)\n", total, w, h);
  return lines;
}

int XF86VidModeLockModeSwitch(Display* dpy, int screen) {
  (void)dpy;
  (void)screen;
  fprintf(stderr, "[VEXA][VIDMODE] lock_mode_switch\n");
  return 1;
}

int XF86VidModeSwitchToMode(Display* dpy, int screen, XF86VidModeModeInfo* mode) {
  (void)dpy;
  (void)screen;
  (void)mode;
  fprintf(stderr, "[VEXA][VIDMODE] switch_to_mode\n");
  return 1;
}

int XF86VidModeGetViewPort(Display* dpy, int screen, short* x, short* y) {
  (void)dpy;
  (void)screen;
  if (x) {
    *x = 0;
  }
  if (y) {
    *y = 0;
  }
  return 1;
}

int XF86VidModeGetAllViewPorts(Display* dpy, int screen, short** viewports, int* count) {
  (void)dpy;
  (void)screen;
  if (viewports) {
    *viewports = NULL;
  }
  if (count) {
    *count = 0;
  }
  return 0;
}