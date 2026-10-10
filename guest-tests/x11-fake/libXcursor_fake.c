/* Guest libXcursor.so.1. Theme files are not on the device, so library cursor
 * loads return NULL. XcursorImageCreate leaves pixels NULL: Godot stores its
 * own buffer there and frees it. Destroy frees only the image struct. */
#include <X11/Xlib.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct XcursorImage {
  uint32_t version;
  uint32_t size;
  uint32_t width;
  uint32_t height;
  uint32_t xhot;
  uint32_t yhot;
  uint32_t delay;
  uint32_t* pixels;
} XcursorImage;

_Static_assert(sizeof(XcursorImage) == 40, "XcursorImage size");
_Static_assert(offsetof(XcursorImage, pixels) == 32, "XcursorImage.pixels");

int XcursorGetDefaultSize(Display* dpy) {
  (void)dpy;
  return 24;
}

char* XcursorGetTheme(Display* dpy) {
  (void)dpy;
  return NULL;
}

XcursorImage* XcursorImageCreate(int width, int height) {
  XcursorImage* image;
  if (width < 1 || height < 1 || width > 1024 || height > 1024) {
    return NULL;
  }
  image = (XcursorImage*)calloc(1, sizeof(XcursorImage));
  if (!image) {
    return NULL;
  }
  image->version = 1;
  image->size = (uint32_t)(width > height ? width : height);
  image->width = (uint32_t)width;
  image->height = (uint32_t)height;
  /* Godot replaces pixels with its own buffer and memfrees that buffer
   * before XcursorImageDestroy. Upstream libXcursor keeps pixels inside
   * the image allocation and destroy frees only the image. */
  image->pixels = NULL;
  return image;
}

void XcursorImageDestroy(XcursorImage* image) {
  if (!image) {
    return;
  }
  free(image);
}

Cursor XcursorImageLoadCursor(Display* dpy, XcursorImage* image) {
  (void)dpy;
  (void)image;
  return 1;
}

XcursorImage* XcursorLibraryLoadImage(const char* library, const char* theme, int size) {
  (void)library;
  (void)theme;
  (void)size;
  return NULL;
}
