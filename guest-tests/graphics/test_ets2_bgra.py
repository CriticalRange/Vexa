"""Build a native GLES regression probe using the actual FEX upload code.

Run the resulting executable on Android with adb shell, without starting FEX.
"""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
fex = root.parent / 'FEX/ThunkLibs/libGL'
out = root / 'logs/ets2-fix'
host = (fex / 'libGL_Host.cpp').read_text()
support = host[host.index('struct VexaBCSource {'):host.index('void VexaBCLogOnce(')]
source = r'''
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cassert>
static GLenum VexaUnorm16Storage(GLenum f) { return f; }
struct VexaUploadTrace { VexaUploadTrace(GLenum, GLenum, GLsizei, GLsizei) {} };
''' + support + '\n#include "' + (fex / 'unorm16_texture.inc').as_posix() + '"\n' + r'''
static void VexaRebindUploadThunks() {}
int main(int argc, char**) {
  const bool rgb = argc > 1;
  EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  assert(eglInitialize(d, nullptr, nullptr));
  EGLint attrs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
  EGLConfig cfg; EGLint n;
  assert(eglChooseConfig(d, attrs, &cfg, 1, &n) && n);
  EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  EGLContext c = eglCreateContext(d, cfg, EGL_NO_CONTEXT, ca);
  EGLint sa[] = {EGL_WIDTH, 2, EGL_HEIGHT, 2, EGL_NONE};
  EGLSurface s = eglCreatePbufferSurface(d, cfg, sa);
  assert(eglMakeCurrent(d, s, s, c));
  GLuint tex, fbo, pbo;
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
  glTexStorage2D(GL_TEXTURE_2D, 1, rgb ? GL_RGB8 : GL_RGBA8, 2, 2);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  uint8_t expected[] = {255,0,0,17, 0,255,0,34, 0,0,255,51, 123,45,67,68};
  uint8_t pixels[48] = {};
  for (int y=0; y<2; ++y) for (int x=0; x<2; ++x) {
    auto p = expected + (y*2+x)*4;
    auto q = pixels + ((y+1)*3+x+1)*4;
    q[0]=p[2]; q[1]=p[1]; q[2]=p[0]; q[3]=p[3];
  }
  glGenBuffers(1, &pbo);
  if (rgb) for (int i=3; i<16; i+=4) expected[i]=255;
  for (int buffered=0; buffered<2; ++buffered) {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 1);
    if (buffered) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
      glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(pixels), pixels, GL_STATIC_DRAW);
    }
    VexaGL_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,2,2,0x80E1,GL_UNSIGNED_BYTE,
                          buffered ? reinterpret_cast<void*>(0) : pixels);
    assert(glGetError() == GL_NO_ERROR);
    GLint row, skip, binding;
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &binding);
    assert(row==3 && skip==1 && binding==(buffered ? (GLint)pbo : 0));
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    uint8_t result[16]; glReadPixels(0,0,2,2,GL_RGBA,GL_UNSIGNED_BYTE,result);
    assert(glGetError()==GL_NO_ERROR);
    assert(memcmp(result,expected,16)==0);
  }
  glDeleteBuffers(1, &pbo); glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &tex);
  eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(d, s); eglDestroyContext(d, c); eglTerminate(d);
  puts(rgb ? "PASS BGRA to RGB8, row/skip, client memory and PBO offset zero" :
             "PASS BGRA to RGBA8, alpha, row/skip, client memory and PBO offset zero");
}
'''
out.mkdir(parents=True, exist_ok=True)
cpp = out / 'bgra_probe.cpp'
cpp.write_text(source)
ndk = Path.home() / 'AppData/Local/Android/Sdk/ndk/29.0.14206865'
clang = ndk / 'toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe'
subprocess.run([str(clang), '--target=aarch64-linux-android28', '-std=c++17',
                '-UNDEBUG', str(cpp), '-lEGL', '-lGLESv3', '-o', str(out / 'bgra_probe')], check=True)
