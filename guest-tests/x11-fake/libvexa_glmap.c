/*
 * libvexa_glmap.so - ETS2-only desktop GL shim, loaded through LD_PRELOAD.
 *
 * ETS2 resolves glMapBuffer with dlsym(libGL.so.1). libGL itself must NOT export
 * it: Unity (Megabonk, Vulkan mode) dlsym()s the same name, sees a desktop-GL
 * capable libGL, and crashes in SDL_GetDisplayName. Keeping the symbol in this
 * preload library gives ETS2 the function without changing what Unity sees.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

typedef void* (*MapBufferRangeFn)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
typedef void (*GetBufferParameterivFn)(GLenum, GLenum, GLint*);

__attribute__((visibility("default")))
void* glMapBuffer(GLenum target, GLenum access) {
  static MapBufferRangeFn map_range;
  static GetBufferParameterivFn get_param;
  if (!map_range) map_range = (MapBufferRangeFn)dlsym(RTLD_DEFAULT, "glMapBufferRange");
  if (!get_param) get_param = (GetBufferParameterivFn)dlsym(RTLD_DEFAULT, "glGetBufferParameteriv");
  if (!map_range || !get_param) return NULL;
  GLint size = 0;
  get_param(target, 0x8764 /* GL_BUFFER_SIZE */, &size);
  if (size <= 0) return NULL;
  GLbitfield bits;
  switch (access) {
    case 0x88B8: bits = 0x0001; break;        /* GL_READ_ONLY */
    case 0x88B9: bits = 0x0002; break;        /* GL_WRITE_ONLY */
    default:     bits = 0x0001 | 0x0002; break; /* GL_READ_WRITE */
  }
  return map_range(target, 0, size, bits);
}
