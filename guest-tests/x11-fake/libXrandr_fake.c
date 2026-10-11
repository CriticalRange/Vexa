/* Guest libXrandr.so.2. Reports one RandR 1.2 output with a single mode that
   mirrors the Android surface, so SDL2/Unity see a real display instead of an
   empty mode list. */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef unsigned long RRMode;
typedef unsigned long RRCrtc;
typedef unsigned long RROutput;
typedef unsigned short Rotation;
typedef unsigned short Connection;
typedef unsigned char SubpixelOrder;

#define RR_ROTATE_0 1
#define RR_CONNECTED 0
#define RR_SUBPIXEL_UNKNOWN 0
#define RR_SET_CONFIG_SUCCESS 0

typedef struct {
  int width;
  int height;
  int mwidth;
  int mheight;
} XRRScreenSize;

typedef struct {
  RRMode id;
  unsigned int width;
  unsigned int height;
  unsigned long dotClock;
  unsigned int hSyncStart;
  unsigned int hSyncEnd;
  unsigned int hTotal;
  unsigned int hSkew;
  unsigned int vSyncStart;
  unsigned int vSyncEnd;
  unsigned int vTotal;
  char* name;
  unsigned int nameLength;
  unsigned long modeFlags;
} XRRModeInfo;

typedef struct {
  Time timestamp;
  Time configTimestamp;
  int ncrtc;
  RRCrtc* crtcs;
  int noutput;
  RROutput* outputs;
  int nmode;
  XRRModeInfo* modes;
} XRRScreenResources;

typedef struct {
  Time timestamp;
  RRCrtc crtc;
  char* name;
  int nameLen;
  unsigned long mm_width;
  unsigned long mm_height;
  Connection connection;
  SubpixelOrder subpixel_order;
  int ncrtc;
  RRCrtc* crtcs;
  int nclone;
  RROutput* clones;
  int nmode;
  int npreferred;
  RRMode* modes;
} XRROutputInfo;

typedef struct {
  Time timestamp;
  int x;
  int y;
  unsigned int width;
  unsigned int height;
  RRMode mode;
  Rotation rotation;
  int noutput;
  RROutput* outputs;
  Rotation rotations;
} XRRCrtcInfo;

/* RandR 1.1 config API types (the older interface Unity's display code uses). */
typedef struct {
  int width;
  int height;
  int mwidth;
  int mheight;
} XRRSizeConfig;

typedef struct {
  XRRScreenSize* sizes;
  int nsizes;
  XRRSizeConfig* size_configs;
  int nconfigs;
  short* rates;
  int nrates;
  int current_size;
} XRRScreenConfiguration;

typedef struct {
  Atom name;
  Bool primary;
  Bool automatic;
  int noutput;
  int x;
  int y;
  int width;
  int height;
  int mwidth;
  int mheight;
  RROutput* outputs;
} XRRMonitorInfo;

void XRRFreeScreenResources(XRRScreenResources* res);
void XRRFreeOutputInfo(XRROutputInfo* info);
void XRRFreeScreenConfigInfo(XRRScreenConfiguration* config);
void XRRFreeMonitors(XRRMonitorInfo* monitors);

static void vexa_screen_size(int* width, int* height) {
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

static void vexa_fill_mode(XRRModeInfo* mode, unsigned long id, int width, int height) {
  memset(mode, 0, sizeof(*mode));
  mode->id = id;
  mode->width = (unsigned int)width;
  mode->height = (unsigned int)height;
  mode->hSyncStart = mode->width;
  mode->hSyncEnd = mode->width + 32;
  mode->hTotal = mode->width + 96;
  mode->hSkew = 0;
  mode->vSyncStart = mode->height;
  mode->vSyncEnd = mode->height + 3;
  mode->vTotal = mode->height + 10;
  /* Real pixel clock in Hz so clients derive ~60 Hz refresh. */
  mode->dotClock = (unsigned long)mode->hTotal * (unsigned long)mode->vTotal * 60UL;
  mode->modeFlags = 0;
}

#define VEXA_RANDR_TRACE(...) fprintf(stderr, "[VEXA][RANDR] " __VA_ARGS__)

Bool XRRQueryExtension(Display* dpy, int* event_base, int* error_base) {
  (void)dpy;
  if (event_base) {
    *event_base = 64;
  }
  if (error_base) {
    *error_base = 128;
  }
  VEXA_RANDR_TRACE("query_extension\n");
  return True;
}

Status XRRQueryVersion(Display* dpy, int* major, int* minor) {
  VEXA_RANDR_TRACE("query_version\n");

  (void)dpy;
  if (major) {
    *major = 1;
  }
  if (minor) {
    *minor = 5;
  }
  return 1;
}

XRRScreenResources* XRRGetScreenResources(Display* dpy, Window window) {
  int tw; int th; vexa_screen_size(&tw, &th);
  VEXA_RANDR_TRACE("get_screen_resources %dx%d\n", tw, th);

  (void)dpy;
  (void)window;
  int width;
  int height;
  vexa_screen_size(&width, &height);

  XRRScreenResources* res = (XRRScreenResources*)calloc(1, sizeof(*res));
  if (!res) {
    return NULL;
  }
  res->timestamp = 1;
  res->configTimestamp = 1;
  res->ncrtc = 1;
  res->noutput = 1;
  res->nmode = 2;
  res->crtcs = (RRCrtc*)calloc(1, sizeof(RRCrtc));
  res->outputs = (RROutput*)calloc(1, sizeof(RROutput));
  res->modes = (XRRModeInfo*)calloc(2, sizeof(XRRModeInfo));
  if (!res->crtcs || !res->outputs || !res->modes) {
    XRRFreeScreenResources(res);
    return NULL;
  }
  res->crtcs[0] = 1;
  res->outputs[0] = 1;
  /* The surface mode is current/preferred; 1920x1080 is offered alongside so
     games that persisted a desktop resolution still find it in the list. */
  vexa_fill_mode(&res->modes[0], 1, width, height);
  vexa_fill_mode(&res->modes[1], 2, 1920, 1080);
  for (int i = 0; i < res->nmode; ++i) {
    char name[32];
    const int len = snprintf(name, sizeof(name), "%ux%u_60.00", res->modes[i].width, res->modes[i].height);
    res->modes[i].name = (char*)malloc((size_t)len + 1);
    if (res->modes[i].name) {
      memcpy(res->modes[i].name, name, (size_t)len + 1);
      res->modes[i].nameLength = (unsigned int)len;
    }
  }
  return res;
}

XRRScreenResources* XRRGetScreenResourcesCurrent(Display* dpy, Window window) {
  VEXA_RANDR_TRACE("get_screen_resources_current\n");

  return XRRGetScreenResources(dpy, window);
}

void XRRFreeScreenResources(XRRScreenResources* res) {
  if (!res) {
    return;
  }
  if (res->modes) {
    int i;
    for (i = 0; i < res->nmode; i++) {
      free(res->modes[i].name);
    }
  }
  free(res->modes);
  free(res->outputs);
  free(res->crtcs);
  free(res);
}

XRRCrtcInfo* XRRGetCrtcInfo(Display* dpy, XRRScreenResources* resources, RRCrtc crtc) {
  int tw; int th; vexa_screen_size(&tw, &th);
  VEXA_RANDR_TRACE("get_crtc_info %dx%d\n", tw, th);

  (void)dpy;
  (void)crtc;
  int width;
  int height;
  vexa_screen_size(&width, &height);

  XRRCrtcInfo* info = (XRRCrtcInfo*)calloc(1, sizeof(*info));
  if (!info) {
    return NULL;
  }
  info->timestamp = 1;
  info->x = 0;
  info->y = 0;
  info->width = (unsigned int)width;
  info->height = (unsigned int)height;
  info->mode = resources && resources->nmode > 0 ? resources->modes[0].id : 1;
  info->rotation = RR_ROTATE_0;
  info->rotations = RR_ROTATE_0;
  info->noutput = 1;
  info->outputs = (RROutput*)calloc(1, sizeof(RROutput));
  if (!info->outputs) {
    free(info);
    return NULL;
  }
  info->outputs[0] = 1;
  return info;
}

void XRRFreeCrtcInfo(XRRCrtcInfo* info) {
  if (!info) {
    return;
  }
  free(info->outputs);
  free(info);
}

XRROutputInfo* XRRGetOutputInfo(Display* dpy, XRRScreenResources* resources, RROutput output) {
  VEXA_RANDR_TRACE("get_output_info\n");

  (void)dpy;
  (void)output;
  int width;
  int height;
  vexa_screen_size(&width, &height);

  XRROutputInfo* info = (XRROutputInfo*)calloc(1, sizeof(*info));
  if (!info) {
    return NULL;
  }
  info->timestamp = 1;
  info->crtc = 1;
  {
    static const char name[] = "VEXA";
    const size_t len = sizeof(name) - 1;
    info->name = (char*)malloc(len + 1);
    if (info->name) {
      memcpy(info->name, name, len + 1);
      info->nameLen = (int)len;
    }
  }
  info->mm_width = (unsigned long)(width * 254 / 960);
  info->mm_height = (unsigned long)(height * 254 / 960);
  info->connection = RR_CONNECTED;
  info->subpixel_order = RR_SUBPIXEL_UNKNOWN;
  info->ncrtc = 1;
  info->crtcs = (RRCrtc*)calloc(1, sizeof(RRCrtc));
  info->nclone = 0;
  info->clones = NULL;
  info->nmode = resources ? resources->nmode : 0;
  info->npreferred = 1;
  info->modes = (RRMode*)calloc((size_t)info->nmode, sizeof(RRMode));
  if (!info->crtcs || !info->modes) {
    XRRFreeOutputInfo(info);
    return NULL;
  }
  info->crtcs[0] = 1;
  for (int i = 0; i < info->nmode; ++i) {
    info->modes[i] = resources->modes[i].id;
  }
  return info;
}

void XRRFreeOutputInfo(XRROutputInfo* info) {
  if (!info) {
    return;
  }
  free(info->name);
  free(info->crtcs);
  free(info->clones);
  free(info->modes);
  free(info);
}

RROutput XRRGetOutputPrimary(Display* dpy, Window window) {
  VEXA_RANDR_TRACE("get_output_primary\n");

  (void)dpy;
  (void)window;
  return 1;
}

int XRRSetCrtcConfig(Display* dpy, XRRScreenResources* resources, RRCrtc crtc, Time timestamp, int x, int y, RRMode mode,
                     Rotation rotation, RROutput* outputs, int noutputs) {
  VEXA_RANDR_TRACE("set_crtc_config crtc=%lu mode=%lu %dx%d outputs=%d\n", (unsigned long)crtc,
                   (unsigned long)mode, x, y, noutputs);
  (void)dpy;
  (void)resources;
  (void)crtc;
  (void)timestamp;
  (void)x;
  (void)y;
  (void)mode;
  (void)rotation;
  (void)outputs;
  (void)noutputs;
  return RR_SET_CONFIG_SUCCESS;
}

int XRRSetScreenSize(Display* dpy, Window window, int width, int height, int mmWidth, int mmHeight) {
  VEXA_RANDR_TRACE("set_screen_size %dx%d\n", width, height);
  (void)dpy;
  (void)window;
  (void)width;
  (void)height;
  (void)mmWidth;
  (void)mmHeight;
  return RR_SET_CONFIG_SUCCESS;
}

XRRScreenSize* XRRSizes(Display* dpy, int screen, int* nsizes) {
  VEXA_RANDR_TRACE("sizes\n");

  (void)dpy;
  (void)screen;
  if (!nsizes) {
    return NULL;
  }
  int width;
  int height;
  vexa_screen_size(&width, &height);
  XRRScreenSize* sizes = (XRRScreenSize*)calloc(1, sizeof(*sizes));
  if (!sizes) {
    *nsizes = 0;
    return NULL;
  }
  sizes[0].width = width;
  sizes[0].height = height;
  sizes[0].mwidth = width * 254 / 960;
  sizes[0].mheight = height * 254 / 960;
  *nsizes = 1;
  return sizes;
}

Atom* XRRListOutputProperties(Display* dpy, RROutput output, int* nprop) {
  (void)dpy;
  (void)output;
  if (nprop) {
    *nprop = 0;
  }
  return NULL;
}

int XRRGetOutputProperty(Display* dpy, RROutput output, Atom property, long offset, long length, Bool delete_prop,
                         Bool pending, Atom req_type, Atom* actual_type, int* actual_format,
                         unsigned long* nitems, unsigned long* bytes_after, unsigned char** prop) {
  (void)dpy;
  (void)output;
  (void)property;
  (void)offset;
  (void)length;
  (void)delete_prop;
  (void)pending;
  (void)req_type;
  if (actual_type) {
    *actual_type = 0;
  }
  if (actual_format) {
    *actual_format = 0;
  }
  if (nitems) {
    *nitems = 0;
  }
  if (bytes_after) {
    *bytes_after = 0;
  }
  if (prop) {
    *prop = NULL;
  }
  return 1;
}

/* ---- RandR 1.1 config API -------------------------------------------------
   Older clients (Unity's display enumeration included) use XRRGetScreenInfo
   plus the XRRConfig* accessors rather than the 1.2 resource model. The
   configuration is opaque to callers, so a self-contained struct works. */

XRRScreenConfiguration* XRRGetScreenInfo(Display* dpy, Drawable draw) {
  int tw; int th; vexa_screen_size(&tw, &th);
  VEXA_RANDR_TRACE("get_screen_info(1.1) %dx%d\n", tw, th);

  (void)dpy;
  (void)draw;
  int width;
  int height;
  vexa_screen_size(&width, &height);

  XRRScreenConfiguration* config = (XRRScreenConfiguration*)calloc(1, sizeof(*config));
  if (!config) {
    return NULL;
  }
  config->nsizes = 2;
  config->nconfigs = 2;
  config->nrates = 1;
  config->sizes = (XRRScreenSize*)calloc((size_t)config->nsizes, sizeof(XRRScreenSize));
  config->size_configs = (XRRSizeConfig*)calloc((size_t)config->nconfigs, sizeof(XRRSizeConfig));
  config->rates = (short*)calloc(1, sizeof(short));
  if (!config->sizes || !config->size_configs || !config->rates) {
    XRRFreeScreenConfigInfo(config);
    return NULL;
  }
  const int dims[2][2] = {{width, height}, {1920, 1080}};
  for (int i = 0; i < 2; ++i) {
    config->sizes[i].width = dims[i][0];
    config->sizes[i].height = dims[i][1];
    config->sizes[i].mwidth = dims[i][0] * 254 / 960;
    config->sizes[i].mheight = dims[i][1] * 254 / 960;
    config->size_configs[i].width = dims[i][0];
    config->size_configs[i].height = dims[i][1];
    config->size_configs[i].mwidth = dims[i][0] * 254 / 960;
    config->size_configs[i].mheight = dims[i][1] * 254 / 960;
  }
  config->rates[0] = 60;
  config->current_size = 0;
  return config;
}

void XRRFreeScreenConfigInfo(XRRScreenConfiguration* config) {
  if (!config) {
    return;
  }
  free(config->sizes);
  free(config->size_configs);
  free(config->rates);
  free(config);
}

XRRScreenSize* XRRConfigSizes(XRRScreenConfiguration* config, int* nsizes) {
  VEXA_RANDR_TRACE("config_sizes\n");

  if (!config || !nsizes) {
    return NULL;
  }
  *nsizes = config->nsizes;
  return config->sizes;
}

short* XRRConfigRates(XRRScreenConfiguration* config, int size_index, int* nrates) {
  (void)size_index;
  if (!config || !nrates) {
    return NULL;
  }
  *nrates = config->nrates;
  return config->rates;
}

int XRRConfigCurrentConfiguration(XRRScreenConfiguration* config, Rotation* rotation) {
  VEXA_RANDR_TRACE("config_current_configuration\n");

  if (rotation) {
    *rotation = RR_ROTATE_0;
  }
  return config ? config->current_size : 0;
}

short XRRConfigCurrentRate(XRRScreenConfiguration* config) {
  VEXA_RANDR_TRACE("config_current_rate\n");

  (void)config;
  return 60;
}

Rotation XRRConfigRotations(XRRScreenConfiguration* config, Rotation* current_rotation) {
  (void)config;
  if (current_rotation) {
    *current_rotation = RR_ROTATE_0;
  }
  return RR_ROTATE_0;
}

Time XRRConfigTimes(XRRScreenConfiguration* config, Time* config_timestamp) {
  (void)config;
  if (config_timestamp) {
    *config_timestamp = 1;
  }
  return 1;
}

int XRRSetScreenConfig(Display* dpy, XRRScreenConfiguration* config, Drawable draw, int size_index,
                       Rotation rotation, Time timestamp) {
  VEXA_RANDR_TRACE("set_screen_config idx=%d\n", size_index);
  (void)dpy;
  (void)draw;
  (void)rotation;
  (void)timestamp;
  if (config && size_index >= 0 && size_index < config->nsizes) {
    config->current_size = size_index;
  }
  return RR_SET_CONFIG_SUCCESS;
}

int XRRSetScreenConfigAndRate(Display* dpy, XRRScreenConfiguration* config, Drawable draw, int size_index,
                              Rotation rotation, short rate, Time timestamp) {
  VEXA_RANDR_TRACE("set_screen_config_rate idx=%d rate=%d\n", size_index, (int)rate);
  (void)rate;
  return XRRSetScreenConfig(dpy, config, draw, size_index, rotation, timestamp);
}

int XRRRootToScreen(Display* dpy, Window root) {
  (void)dpy;
  (void)root;
  return 0;
}

void XRRSelectInput(Display* dpy, Window window, int mask) {
  VEXA_RANDR_TRACE("select_input mask=0x%x\n", mask);
  (void)dpy;
  (void)window;
  (void)mask;
}

int XRRUpdateConfiguration(XEvent* event) {
  VEXA_RANDR_TRACE("update_configuration type=%d\n", event ? event->type : -1);
  (void)event;
  return 0;
}

/* ---- RandR 1.5 monitor API ----------------------------------------------- */

XRRMonitorInfo* XRRGetMonitors(Display* dpy, Window window, Bool get_active, int* nmonitors) {
  int tw; int th; vexa_screen_size(&tw, &th);
  VEXA_RANDR_TRACE("get_monitors %dx%d\n", tw, th);

  (void)dpy;
  (void)window;
  (void)get_active;
  if (!nmonitors) {
    return NULL;
  }
  int width;
  int height;
  vexa_screen_size(&width, &height);
  XRRMonitorInfo* monitors = (XRRMonitorInfo*)calloc(1, sizeof(*monitors));
  if (!monitors) {
    *nmonitors = 0;
    return NULL;
  }
  monitors->name = 0;
  monitors->primary = True;
  monitors->automatic = True;
  monitors->noutput = 1;
  monitors->x = 0;
  monitors->y = 0;
  monitors->width = width;
  monitors->height = height;
  monitors->mwidth = width * 254 / 960;
  monitors->mheight = height * 254 / 960;
  monitors->outputs = (RROutput*)calloc(1, sizeof(RROutput));
  if (!monitors->outputs) {
    free(monitors);
    *nmonitors = 0;
    return NULL;
  }
  monitors->outputs[0] = 1;
  *nmonitors = 1;
  return monitors;
}

XRRMonitorInfo* XRRAllocateMonitors(Display* dpy, int nmonitors) {
  (void)dpy;
  if (nmonitors <= 0) {
    return NULL;
  }
  return (XRRMonitorInfo*)calloc((size_t)nmonitors, sizeof(XRRMonitorInfo));
}

void XRRFreeMonitors(XRRMonitorInfo* monitors) {
  if (!monitors) {
    return;
  }
  free(monitors->outputs);
  free(monitors);
}
