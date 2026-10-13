// Native Android regression test of the actual i386 host bridge helpers.
// Compile with NDK clang++, -std=c++20 -static-libstdc++, and -I../FEX/ThunkLibs/libGL.
#include <GLES3/gl3.h>
#include <sys/mman.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

static int allocations, frees;
static void* Allocate(uint32_t size) {
  static uintptr_t hint = 0x20000000;
  void* result = mmap(reinterpret_cast<void*>(hint), 4096, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  hint += 0x10000;
  assert(result != MAP_FAILED && reinterpret_cast<uintptr_t>(result) < UINT32_MAX && size <= 4096);
  ++allocations;
  return result;
}
static void Release(void* ptr) { assert(munmap(ptr, 4096) == 0); ++frees; }
static auto GuestMalloc = Allocate;
static auto GuestFree = Release;
static unsigned char driver[32];
static bool mapped;
extern "C" void glGetIntegerv(GLenum, GLint* out) { *out = 0; }
extern "C" GLboolean glIsBuffer(GLuint name) { return name == 1; }
extern "C" void glBindBuffer(GLenum, GLuint) {}
extern "C" void glDeleteBuffers(GLsizei, const GLuint*) { mapped = false; }
static std::vector<std::string> varyings;
extern "C" void* glMapBufferRange(GLenum, GLintptr offset, GLsizeiptr size, GLbitfield) {
  if (mapped || offset < 0 || size <= 0 || offset + size > 32) return nullptr;
  mapped = true;
  return driver + offset;
}
extern "C" void glGetBufferPointerv(GLenum, GLenum, void** out) { *out = mapped ? driver : nullptr; }
extern "C" GLboolean glUnmapBuffer(GLenum) { mapped = false; return GL_TRUE; }
extern "C" void glFlushMappedBufferRange(GLenum, GLintptr, GLsizeiptr) {}
extern "C" void glTransformFeedbackVaryings(GLuint, GLsizei count, const GLchar* const* names, GLenum) {
  varyings.clear();
  for (int i = 0; i < count; ++i) varyings.emplace_back(names[i]);
}
#define IS_32BIT_THUNK
#include "android_gl_guest32.h"

int main() {
  const auto text = VexaGuestString("renderer");
  assert(reinterpret_cast<uintptr_t>(text) < UINT32_MAX && !strcmp(text, "renderer"));
  assert(VexaGuestString("renderer") == text);
  uint32_t names[] = {uint32_t(reinterpret_cast<uintptr_t>(text)), uint32_t(reinterpret_cast<uintptr_t>(text))};
  VexaTransformFeedbackVaryings32(0, 2, reinterpret_cast<const char* const*>(names), 0);
  assert(varyings.size() == 2 && varyings[1] == "renderer");
  const auto nativeSync = reinterpret_cast<GLsync>(uintptr_t{0x1234567890});
  auto id = VexaExportSync(nativeSync);
  assert(reinterpret_cast<uintptr_t>(id) < UINT32_MAX && VexaImportSync(id) == nativeSync);
  assert(VexaImportSync(id, true) == nativeSync && VexaImportSync(id) == nullptr);
  assert(VexaExportSync(nullptr) == nullptr);
  memset(driver, 7, sizeof(driver));
  auto data = static_cast<unsigned char*>(fexfn_impl_libGL_glMapBufferRange(0, 0, 32, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));
  assert(data && data[31] == 7);
  struct { uint32_t pointer, canary; } output{0, 0xabcdef01};
  VexaGetBufferPointer32(0, GL_BUFFER_MAP_POINTER, reinterpret_cast<void**>(&output.pointer));
  assert(output.pointer == reinterpret_cast<uintptr_t>(data) && output.canary == 0xabcdef01);
  data[3] = 11;
  assert(driver[3] == 7);
  assert(fexfn_impl_libGL_glUnmapBuffer(0) == GL_TRUE && driver[3] == 11 && frees == 1);
  data = static_cast<unsigned char*>(fexfn_impl_libGL_glMapBufferRange(0, 0, 32, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT));
  memset(data, 19, 32);
  fexfn_impl_libGL_glFlushMappedBufferRange(0, 4, 2);
  assert(driver[4] == 19 && driver[6] == 7);
  fexfn_impl_libGL_glFlushMappedBufferRange(0, 31, 2); // Must not overrun shadow or driver.
  fexfn_impl_libGL_glUnmapBuffer(0);
  assert(driver[6] == 7 && frees == 2 && VexaMappedBuffers.empty());
  assert(fexfn_impl_libGL_glMapBufferRange(0, 0, -1, 0) == nullptr);
  assert(fexfn_impl_libGL_glMapBufferRange(0, 0, 32, GL_MAP_WRITE_BIT));
  GLuint buffer = 1;
  VexaDeleteBuffers32(1, &buffer);
  assert(frees == 3 && VexaMappedBuffers.empty() && !mapped);
  puts("GL_GUEST32_HELPERS_PASS");
}
