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
  unsigned int dotclock;
  unsigned short hdisplay;
  unsigned short hsyncstart;
  unsigned short hsyncend;
  unsigned short htotal;
  unsigned short hskew;
  unsigned short vdisplay;
  unsigned short vsyncstart;
  unsigned short vsyncend;
  unsigned short vtotal;
  unsigned int flags;
  int privsize;
  long* c_private;
} XF86VidModeModeInfo;

typedef struct {
  unsigned short hdisplay;
  unsigned short hsyncstart;
  unsigned short hsyncend;
  unsigned short htotal;
  unsigned short hskew;
  unsigned short vdisplay;
  unsigned short vsyncstart;
  unsigned short vsyncend;
  unsigned short vtotal;
  unsigned int flags;
  int privsize;
  long* c_private;
} XF86VidModeModeLine;

typedef char vexa_assert_vidmode_hdisplay[(offsetof(XF86VidModeModeInfo, hdisplay) == 4) ? 1 : -1];
typedef char vexa_assert_vidmode_vdisplay[(offsetof(XF86VidModeModeInfo, vdisplay) == 14) ? 1 : -1];
typedef char vexa_assert_vidmode_flags[(offsetof(XF86VidModeModeInfo, flags) == 24) ? 1 : -1];

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

/* The list SDL/GameMaker will walk: current surface size first, then common
   resolutions so games that look for a specific mode (e.g. 640x480) find it. */
static const int kExtra[4][2] = {{1920, 1080}, {1280, 720}, {1024, 768}, {640, 480}};

static void vexa_fill_mode(XF86VidModeModeInfo* m, int w, int h) {
  memset(m, 0, sizeof(*m));
  /* dotclock in kHz so dotclock * 1000 / (htotal * vtotal) == 60 Hz */
  m->dotclock = (unsigned int)(((w + 96) * (h + 10) * 60) / 1000);
  m->hdisplay = (unsigned short)w;
  m->hsyncstart = (unsigned short)w;
  m->hsyncend = (unsigned short)(w + 32);
  m->htotal = (unsigned short)(w + 96);
  m->hskew = 0;
  m->vdisplay = (unsigned short)h;
  m->vsyncstart = (unsigned short)h;
  m->vsyncend = (unsigned short)(h + 3);
  m->vtotal = (unsigned short)(h + 10);
  m->flags = 0;
  m->privsize = 0;
  m->c_private = NULL;
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

Bool XF86VidModeGetModeLine(Display* dpy, int screen, int* dotclock, XF86VidModeModeLine* mode) {
  (void)dpy;
  (void)screen;
  int w;
  int h;
  vexa_vidmode_size(&w, &h);
  if (dotclock) {
    *dotclock = ((w + 96) * (h + 10) * 60) / 1000;
  }
  if (!mode) {
    return False;
  }
  memset(mode, 0, sizeof(*mode));
  mode->hdisplay = (unsigned short)w;
  mode->hsyncstart = (unsigned short)w;
  mode->hsyncend = (unsigned short)(w + 32);
  mode->htotal = (unsigned short)(w + 96);
  mode->hskew = 0;
  mode->vdisplay = (unsigned short)h;
  mode->vsyncstart = (unsigned short)h;
  mode->vsyncend = (unsigned short)(h + 3);
  mode->vtotal = (unsigned short)(h + 10);
  fprintf(stderr, "[VEXA][VIDMODE] get_mode_line -> %dx%d\n", w, h);
  return True;
}

Bool XF86VidModeGetAllModeLines(Display* dpy, int screen, int* count, XF86VidModeModeInfo*** modesinfo) {
  (void)dpy;
  (void)screen;
  int w;
  int h;
  vexa_vidmode_size(&w, &h);
  const int total = 1 + (int)(sizeof(kExtra) / sizeof(kExtra[0]));
  /* Single contiguous allocation so callers that free with a single XFree(modes)
     release both the pointer table and the mode structs. */
  const size_t ptrs_bytes = (size_t)total * sizeof(XF86VidModeModeInfo*);
  const size_t modes_bytes = (size_t)total * sizeof(XF86VidModeModeInfo);
  XF86VidModeModeInfo** lines = (XF86VidModeModeInfo**)calloc(1, ptrs_bytes + modes_bytes);
  if (!lines) {
    if (count) {
      *count = 0;
    }
    if (modesinfo) {
      *modesinfo = NULL;
    }
    return False;
  }
  XF86VidModeModeInfo* pool = (XF86VidModeModeInfo*)((char*)lines + ptrs_bytes);
  lines[0] = &pool[0];
  vexa_fill_mode(lines[0], w, h);
  for (int i = 0; i < total - 1; ++i) {
    lines[i + 1] = &pool[i + 1];
    vexa_fill_mode(lines[i + 1], kExtra[i][0], kExtra[i][1]);
  }
  if (count) {
    *count = total;
  }
  if (modesinfo) {
    *modesinfo = lines;
  }
  fprintf(stderr, "[VEXA][VIDMODE] get_all_mode_lines -> %d modes (current %dx%d)\n", total, w, h);
  return True;
}

Bool XF86VidModeLockModeSwitch(Display* dpy, int screen, int lock) {
  (void)dpy;
  (void)screen;
  (void)lock;
  fprintf(stderr, "[VEXA][VIDMODE] lock_mode_switch\n");
  return True;
}

Bool XF86VidModeSwitchToMode(Display* dpy, int screen, XF86VidModeModeInfo* mode) {
  (void)dpy;
  (void)screen;
  (void)mode;
  fprintf(stderr, "[VEXA][VIDMODE] switch_to_mode\n");
  return True;
}

Bool XF86VidModeGetViewPort(Display* dpy, int screen, int* x, int* y) {
  (void)dpy;
  (void)screen;
  if (x) {
    *x = 0;
  }
  if (y) {
    *y = 0;
  }
  return True;
}

Bool XF86VidModeGetAllViewPorts(Display* dpy, int screen, short** viewports, int* count) {
  (void)dpy;
  (void)screen;
  if (viewports) {
    *viewports = NULL;
  }
  if (count) {
    *count = 0;
  }
  return False;
}

Bool XF86VidModeSetViewPort(Display* dpy, int screen, int x, int y) {
  (void)dpy;
  (void)screen;
  (void)x;
  (void)y;
  fprintf(stderr, "[VEXA][VIDMODE] set_view_port\n");
  return True;
}
