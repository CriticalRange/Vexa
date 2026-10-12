/*
 * Xrandr wire types for the guest libXrandr.so.2 fake.
 *
 * These definitions are copied verbatim from the X.Org Xrandr.h that the guest
 * games were compiled against. The fake used to declare its own copies, and two
 * of them silently drifted from the real layout:
 *
 *   - XRRModeInfo declared hSyncStart..vTotal as `unsigned short`. The real
 *     header uses `unsigned int`, which shifts `name`, `nameLength` and
 *     `modeFlags` and makes every guest compute the display refresh rate wrong
 *     (Unity logged `Desktop is 2712 x 1220 @ 0 Hz` for a mode whose
 *     dotClock/(hTotal*vTotal) is exactly 60).
 *   - XRRCrtcInfo was reordered and given `nmode`/`modes` fields that the real
 *     struct does not have; `noutput`, `outputs` and `rotations` landed on the
 *     wrong offsets.
 *
 * A guest reads these structs directly through the real header, so the layout
 * is part of the ABI. Do not "tidy" these declarations; copy them from Xrandr.h.
 *
 * Source: X11R7.x include/X11/extensions/Xrandr.h
 */
#ifndef VEXA_FAKE_XRANDR_TYPES_H
#define VEXA_FAKE_XRANDR_TYPES_H

#include <stddef.h>

#include <X11/Xlib.h>

typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;

typedef unsigned short Rotation;
typedef unsigned short Connection;
typedef unsigned char SubpixelOrder;

typedef struct {
  int width, height;
  int mwidth, mheight;
} XRRScreenSize;

/* internal representation is private to the library */
typedef struct _XRRScreenConfiguration XRRScreenConfiguration;

typedef unsigned long XRRModeFlags;

typedef struct _XRRModeInfo {
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
  XRRModeFlags modeFlags;
} XRRModeInfo;

typedef struct _XRRScreenResources {
  Time timestamp;
  Time configTimestamp;
  int ncrtc;
  RRCrtc* crtcs;
  int noutput;
  RROutput* outputs;
  int nmode;
  XRRModeInfo* modes;
} XRRScreenResources;

typedef struct _XRROutputInfo {
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

typedef struct _XRRCrtcInfo {
  Time timestamp;
  int x, y;
  unsigned int width, height;
  RRMode mode;
  Rotation rotation;
  int noutput;
  RROutput* outputs;
  Rotation rotations;
  int npossible;
  RROutput* possible;
} XRRCrtcInfo;

/* Layout assertions: a guest compiled against the real Xrandr.h reads these
   offsets, so a mismatch has to fail the build rather than corrupt reads. */
#define VEXA_STATIC_ASSERT(cond, name) typedef char vexa_static_assert_##name[(cond) ? 1 : -1]

VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, id) == 0, modeinfo_id);
VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, width) == 8, modeinfo_width);
VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, height) == 12, modeinfo_height);
VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, dotClock) == 16, modeinfo_dotclock);
VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, hSyncStart) == 24, modeinfo_hsyncstart);
VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, hTotal) == 32, modeinfo_htotal);
VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, vTotal) == 48, modeinfo_vtotal);
VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, name) == 56, modeinfo_name);
VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, nameLength) == 64, modeinfo_namelength);
VEXA_STATIC_ASSERT(offsetof(XRRModeInfo, modeFlags) == 72, modeinfo_modeflags);
VEXA_STATIC_ASSERT(sizeof(XRRModeInfo) == 80, modeinfo_size);

VEXA_STATIC_ASSERT(offsetof(XRRCrtcInfo, x) == 8, crtcinfo_x);
VEXA_STATIC_ASSERT(offsetof(XRRCrtcInfo, mode) == 24, crtcinfo_mode);
VEXA_STATIC_ASSERT(offsetof(XRRCrtcInfo, rotation) == 32, crtcinfo_rotation);
VEXA_STATIC_ASSERT(offsetof(XRRCrtcInfo, noutput) == 36, crtcinfo_noutput);
VEXA_STATIC_ASSERT(offsetof(XRRCrtcInfo, outputs) == 40, crtcinfo_outputs);
VEXA_STATIC_ASSERT(offsetof(XRRCrtcInfo, rotations) == 48, crtcinfo_rotations);
VEXA_STATIC_ASSERT(offsetof(XRRCrtcInfo, npossible) == 52, crtcinfo_npossible);
VEXA_STATIC_ASSERT(offsetof(XRRCrtcInfo, possible) == 56, crtcinfo_possible);
VEXA_STATIC_ASSERT(sizeof(XRRCrtcInfo) == 64, crtcinfo_size);

VEXA_STATIC_ASSERT(offsetof(XRROutputInfo, nameLen) == 24, outputinfo_namelen);
VEXA_STATIC_ASSERT(offsetof(XRROutputInfo, mm_width) == 32, outputinfo_mmwidth);
VEXA_STATIC_ASSERT(offsetof(XRROutputInfo, connection) == 48, outputinfo_connection);
VEXA_STATIC_ASSERT(offsetof(XRROutputInfo, ncrtc) == 52, outputinfo_ncrtc);
VEXA_STATIC_ASSERT(offsetof(XRROutputInfo, nmode) == 80, outputinfo_nmode);
VEXA_STATIC_ASSERT(offsetof(XRROutputInfo, npreferred) == 84, outputinfo_npreferred);
VEXA_STATIC_ASSERT(offsetof(XRROutputInfo, modes) == 88, outputinfo_modes);
VEXA_STATIC_ASSERT(sizeof(XRROutputInfo) == 96, outputinfo_size);

VEXA_STATIC_ASSERT(offsetof(XRRScreenResources, nmode) == 48, resources_nmode);
VEXA_STATIC_ASSERT(offsetof(XRRScreenResources, modes) == 56, resources_modes);

#undef VEXA_STATIC_ASSERT

#endif /* VEXA_FAKE_XRANDR_TYPES_H */
