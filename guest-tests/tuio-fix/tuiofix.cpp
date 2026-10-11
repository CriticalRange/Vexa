// libvexa_tuiofix.so — LD_PRELOAD shim for the Qt TUIO plugin.
//
// qtuiohandler delivers every touch event to the focused window (or the
// first top-level window) and computes the position as
// win->pos() + normalized * win->size(). For a non-fullscreen window (e.g.
// Prism's centered Settings dialog) that delivers the event to the wrong
// window and lets Qt's modal redirection clamp out-of-bounds positions onto
// edge widgets, so tapping empty screen space still hits dialog items.
//
// This shim interposes QWindowSystemInterface::handleTouchEvent, recovers the
// host-normalized coordinate, maps it onto the primary screen, and retargets
// the event to the smallest visible top-level window containing that point.

// Compiled -nostdlib: declare the handful of libc/libdl entry points we use so
// the .so carries no DT_NEEDED and no version refs — everything binds against
// the already-loaded Ubuntu glibc at preload time.
extern "C" {
int fprintf(void *stream, const char *fmt, ...);
void free(void *ptr);
void *dlsym(void *handle, const char *symbol);
int open(const char *path, int flags, ...);
long write(int fd, const void *buf, unsigned long n);
int close(int fd);
int rename(const char *from, const char *to);
int *__errno_location(void);
char *getenv(const char *name);
int snprintf(char *buf, unsigned long n, const char *fmt, ...);
extern void *stderr;
}
#define VEXA_RTLD_NEXT (reinterpret_cast<void *>(-1))
#define VEXA_RTLD_DEFAULT (nullptr)

#include <cstdint>

struct VexaRect { int x, y, w, h; };
struct VexaPointF { double x, y; };
struct VexaRectF { double x, y, w, h; };
struct VexaTouchPoint {
  int id;
  int _pad;
  int64_t uniqueId;
  VexaPointF normalPosition;
  VexaRectF area; // +32: ellipse rect centered at the global position
};
struct VexaList { void *d; void *ptr; long size; }; // Qt6 QList<T>

struct VexaQRect { int x1, y1, x2, y2; };
using GeomFn = VexaQRect (*)(void *);
using BoolFn = bool (*)(void *);
using HandleTouchFn = bool (*)(void *, const void *, const VexaList *, int);

static void *VexaSym(const char *name) {
  void *fn = dlsym(VEXA_RTLD_NEXT, name);
  if (!fn) {
    fn = dlsym(VEXA_RTLD_DEFAULT, name);
  }
  return fn;
}

// QRect is laid out as inclusive corners (x1,y1,x2,y2); convert to x,y,w,h.
static VexaRect VexaFromQRect(VexaQRect r) {
  return VexaRect{r.x1, r.y1, r.x2 - r.x1 + 1, r.y2 - r.y1 + 1};
}

static VexaRect VexaWindowGeometry(void *window) {
  static GeomFn fn = reinterpret_cast<GeomFn>(VexaSym("_ZNK7QWindow8geometryEv"));
  VexaRect empty{0, 0, 0, 0};
  return fn ? VexaFromQRect(fn(window)) : empty;
}

static bool VexaWindowVisible(void *window) {
  static BoolFn fn = reinterpret_cast<BoolFn>(VexaSym("_ZNK7QWindow9isVisibleEv"));
  return fn ? fn(window) : false;
}

static VexaRect VexaScreenGeometry() {
  static GeomFn screenGeom = reinterpret_cast<GeomFn>(VexaSym("_ZNK7QScreen8geometryEv"));
  using PrimaryFn = void *(*)();
  static PrimaryFn primary = reinterpret_cast<PrimaryFn>(VexaSym("_ZN15QGuiApplication13primaryScreenEv"));
  VexaRect empty{0, 0, 0, 0};
  if (!screenGeom || !primary) {
    return empty;
  }
  void *screen = primary();
  return screen ? VexaFromQRect(screenGeom(screen)) : empty;
}

static void VexaReleaseList(VexaList &list) {
  // QList data is refcounted through QArrayData (first int) and owns `ptr`.
  if (list.d) {
    int &ref = *reinterpret_cast<int *>(list.d);
    if (ref > 0 && __sync_sub_and_fetch(&ref, 1) == 0) {
      free(list.d);
    }
  }
}

static void *VexaPickWindow(double gx, double gy, void *fallback) {
  using TopLevelFn = void (*)(VexaList *);
  static TopLevelFn topLevel = reinterpret_cast<TopLevelFn>(VexaSym("_ZN15QGuiApplication15topLevelWindowsEv"));
  if (!topLevel) {
    return fallback;
  }
  VexaList list{};
  topLevel(&list);
  void *best = nullptr;
  int64_t bestArea = INT64_MAX;
  void **wins = reinterpret_cast<void **>(list.ptr);
  static int dumpLogged = 0;
  const bool dump = dumpLogged++ < 16;
  for (long i = 0; i < list.size; ++i) {
    void *w = wins[i];
    if (!w || !VexaWindowVisible(w)) {
      continue;
    }
    VexaRect g = VexaWindowGeometry(w);
    if (dump) {
      fprintf(stderr, "[VEXA][TUIOFIX]   cand %p geo=%d,%d %dx%d\n", w, g.x, g.y, g.w, g.h);
    }
    if (gx >= g.x && gx < g.x + g.w && gy >= g.y && gy < g.y + g.h) {
      int64_t area = static_cast<int64_t>(g.w) * g.h;
      if (area < bestArea) {
        bestArea = area;
        best = w;
      }
    }
  }
  VexaReleaseList(list);
  return best ? best : fallback;
}

extern "C" bool
_ZN22QWindowSystemInterface16handleTouchEventINS_15DefaultDeliveryEEEbP7QWindowPK15QPointingDeviceRK5QListINS_10TouchPointEE6QFlagsIN2Qt16KeyboardModifierEE(
    void *window, const void *device, const VexaList *points, int mods) {
  static HandleTouchFn real =
      reinterpret_cast<HandleTouchFn>(VexaSym("_ZN22QWindowSystemInterface16handleTouchEventINS_15DefaultDeliveryEEEbP7QWindowPK15QPointingDeviceRK5QListINS_10TouchPointEE6QFlagsIN2Qt16KeyboardModifierEE"));
  if (!real) {
    return false;
  }
  void *target = window;
  if (window && points && points->ptr && points->size > 0) {
    const VexaTouchPoint *tp = reinterpret_cast<const VexaTouchPoint *>(points->ptr);
    const VexaRect w0 = VexaWindowGeometry(window);
    const VexaRect scr = VexaScreenGeometry();
    if (w0.w > 0 && w0.h > 0 && scr.w > 0 && scr.h > 0) {
      // The plugin wrote area.center = w0.pos + n * w0.size. Recover n and
      // re-expand it onto the real screen so dialogs can't squash the range.
      const double gx = tp->area.x + tp->area.w / 2.0;
      const double gy = tp->area.y + tp->area.h / 2.0;
      const double nx = (gx - w0.x) / w0.w;
      const double ny = (gy - w0.y) / w0.h;
      const double sx = scr.x + nx * scr.w;
      const double sy = scr.y + ny * scr.h;
      void *picked = VexaPickWindow(sx, sy, window);
      {
        static int logged = 0;
        if (logged++ < 16) {
          fprintf(stderr,
              "[VEXA][TUIOFIX] win=%p w0=%d,%d %dx%d scr=%d,%d %dx%d g=%.0f,%.0f s=%.0f,%.0f pick=%p\n",
              window, w0.x, w0.y, w0.w, w0.h, scr.x, scr.y, scr.w, scr.h, gx, gy, sx, sy, picked);
        }
      }
      target = picked;
      // The plugin placed the point relative to the focused window's rect;
      // rewrite it to the true screen position so Qt maps it into `target`.
      VexaTouchPoint *mtp = const_cast<VexaTouchPoint *>(tp);
      for (long i = 0; i < points->size; ++i) {
        const double pgx = mtp[i].area.x + mtp[i].area.w / 2.0;
        const double pgy = mtp[i].area.y + mtp[i].area.h / 2.0;
        const double sxi = scr.x + ((pgx - w0.x) / w0.w) * scr.w;
        const double syi = scr.y + ((pgy - w0.y) / w0.h) * scr.h;
        mtp[i].area.x = sxi - mtp[i].area.w / 2.0;
        mtp[i].area.y = syi - mtp[i].area.h / 2.0;
      }
    }
  }
  return real(target, device, points, mods);
}

// ---------------------------------------------------------------------------
// QDesktopServices::openUrl bridge.
//
// The guest has no browser, and Qt's xdg-open path spawns a process with a
// clone() form FEX does not support, which kills the whole guest (seen on
// Prism's "Add Microsoft account" flow). Intercept openUrl, hand the URL to
// the Android host through /tmp/vexa-open-url, and let GameActivity fire an
// ACTION_VIEW intent so the phone's own browser opens it.
// ---------------------------------------------------------------------------
using ToEncodedFn = void (*)(VexaList *ret, const void *url, int options);

extern "C" bool _ZN16QDesktopServices7openUrlERK4QUrl(const void *url) {
  static ToEncodedFn toEncoded = reinterpret_cast<ToEncodedFn>(
      VexaSym("_ZNK4QUrl9toEncodedE12QUrlTwoFlagsINS_19UrlFormattingOptionENS_25ComponentFormattingOptionEE"));
  if (!toEncoded || !url) {
    return false;
  }
  VexaList bytes{};
  toEncoded(&bytes, url, 0);
  bool ok = false;
  if (bytes.ptr && bytes.size > 0) {
    // /tmp is not writable for the guest (EACCES); TMPDIR is.
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) {
      dir = "/tmp";
    }
    char tmp[512];
    char dst[512];
    snprintf(tmp, sizeof(tmp), "%s/vexa-open-url.tmp", dir);
    snprintf(dst, sizeof(dst), "%s/vexa-open-url", dir);
    // O_WRONLY | O_CREAT | O_TRUNC
    int fd = open(tmp, 0x1 | 0x40 | 0x200, 0644);
    int openErr = fd < 0 ? *__errno_location() : 0;
    int renameErr = 0;
    if (fd >= 0) {
      write(fd, bytes.ptr, static_cast<unsigned long>(bytes.size));
      write(fd, "\n", 1);
      close(fd);
      ok = rename(tmp, dst) == 0;
      if (!ok) renameErr = *__errno_location();
    }
    fprintf(stderr, "[VEXA][OPENURL] %.*s -> host (%d) openErr=%d renameErr=%d\n",
            static_cast<int>(bytes.size), static_cast<const char *>(bytes.ptr), ok ? 1 : 0,
            openErr, renameErr);
  }
  VexaReleaseList(bytes);
  return ok;
}

// NOTE: self-kill diagnostics (interposing raise/abort/pthread_kill/sigaction
// to dump a guest backtrace) caused a startup deadlock — glibc's backtrace
// path grabs the dynamic-loader lock, which another thread may hold during
// early dlopen/init. Keep the shim limited to touch retarget + openUrl.
