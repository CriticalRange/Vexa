// End-to-end i386 dynamic loader, guest callback and GL thunk regression probe.
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
#define PROC(type, name) type name = (type)getproc((const GLubyte*)#name); CHECK(name)
int main(void) {
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);
  puts("I386_PROBE_START");
  void* lib = dlopen("libGL.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!lib) fprintf(stderr, "dlopen: %s\n", dlerror());
  CHECK(lib);
  void* (*getproc)(const GLubyte*) = dlsym(lib, "glXGetProcAddress");
  void* (*create)(void*, void*, void*, int) = dlsym(lib, "glXCreateContext");
  int (*bind)(void*, unsigned long, void*) = dlsym(lib, "glXMakeCurrent");
  void (*destroy)(void*, void*) = dlsym(lib, "glXDestroyContext");
  CHECK(getproc && create && bind && destroy);
  void* ctx = create(NULL, NULL, NULL, 1);
  CHECK(ctx && bind(NULL, 1, ctx));
  typedef const GLubyte* (*GetString)(GLenum);
  PROC(GetString, glGetString);
  CHECK(strlen((const char*)glGetString(GL_RENDERER)) > 0);
  printf("renderer=%s\n", glGetString(GL_RENDERER));
  typedef void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
  typedef void (*Clear)(GLbitfield);
  typedef void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
  PROC(ClearColor, glClearColor);
  PROC(Clear, glClear);
  PROC(ReadPixels, glReadPixels);
  glClearColor(1, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  unsigned char pixel[4] = {0};
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  CHECK(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255);
  PROC(PFNGLCREATESHADERPROC, glCreateShader);
  PROC(PFNGLSHADERSOURCEPROC, glShaderSource);
  PROC(PFNGLCOMPILESHADERPROC, glCompileShader);
  PROC(PFNGLGETSHADERIVPROC, glGetShaderiv);
  PROC(PFNGLDELETESHADERPROC, glDeleteShader);
  GLuint shader = glCreateShader(GL_VERTEX_SHADER);
  const char* source[] = {"#version 300 es\n", "void main(){gl_Position=vec4(0.0);}"};
  glShaderSource(shader, 2, source, NULL);
  glCompileShader(shader);
  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  CHECK(compiled);
  glDeleteShader(shader);
  PROC(PFNGLGENBUFFERSPROC, glGenBuffers);
  PROC(PFNGLBINDBUFFERPROC, glBindBuffer);
  PROC(PFNGLBUFFERDATAPROC, glBufferData);
  PROC(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange);
  PROC(PFNGLUNMAPBUFFERPROC, glUnmapBuffer);
  PROC(PFNGLGETBUFFERPOINTERVPROC, glGetBufferPointerv);
  PROC(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  uint32_t initial[] = {11, 22, 33, 44};
  glBufferData(GL_ARRAY_BUFFER, sizeof(initial), initial, GL_DYNAMIC_DRAW);
  uint32_t* data = glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(initial), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
  CHECK(data && data[2] == 33);
  struct { void* pointer; uint32_t canary; } out = {NULL, 0xabcdef01};
  glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &out.pointer);
  CHECK(out.pointer == data && out.canary == 0xabcdef01);
  data[2] = 99;
  CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));
  data = glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(initial), GL_MAP_READ_BIT);
  CHECK(data && data[2] == 99);
  CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));
  // Deleting a still-mapped buffer implicitly unmaps it and releases its shadow.
  CHECK(glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(initial), GL_MAP_WRITE_BIT));
  glDeleteBuffers(1, &buffer);
  PROC(PFNGLFENCESYNCPROC, glFenceSync);
  PROC(PFNGLISSYNCPROC, glIsSync);
  PROC(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync);
  PROC(PFNGLDELETESYNCPROC, glDeleteSync);
  GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  CHECK(sync && glIsSync(sync));
  GLenum wait = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL);
  CHECK(wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED);
  glDeleteSync(sync);
  CHECK(!glIsSync(sync));
  bind(NULL, 0, NULL);
  destroy(NULL, ctx);
  printf("GL_PROBE_PASS bits=%zu\n", sizeof(void*) * 8);
  return 0;
}
