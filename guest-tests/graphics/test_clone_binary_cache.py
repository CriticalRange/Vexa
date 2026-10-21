"""Build an Android regression probe from the actual GL clone implementation."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
fex = root.parent / "FEX/ThunkLibs/libGL"
out = root / "logs/clone-cache-20261021"
host = (fex / "libGL_Host.cpp").read_text()
clone = host[host.index("// ---- separable-pipeline vertex clone"):host.index("// Validates each bound pipeline once.")]
source = r'''
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <chrono>
#include "clone_binary_cache.h"
''' + clone + r'''
static const char* vs = R"(#version 310 es
precision highp float;
uniform vec4 tint;
out vec4 color;
out vec4 unusedColor;
void main() {
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
  color = tint;
  unusedColor = vec4(1.0);
}
)";
static const char* fs = R"(#version 310 es
precision highp float;
in vec4 color;
layout(location=0) out vec4 outputColor;
void main() { outputColor = color; }
)";
static GLuint Program(GLenum stage, const char* src) {
  GLuint p = glCreateShaderProgramv(stage, 1, &src);
  GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok); assert(ok);
  return p;
}
static GLuint Clone(GLuint fragment, const char* src) {
  GLuint vertex = Program(GL_VERTEX_SHADER, src), pipe;
  g_vexa_prog_direct_source[vertex] = {GL_VERTEX_SHADER, src};
  glGenProgramPipelines(1, &pipe);
  glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vertex);
  glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, fragment);
  GLuint result = VexaBuildStrippedVertexProgram(pipe);
  assert(result);
  glDeleteProgramPipelines(1, &pipe);
  glDeleteProgram(vertex);
  return result;
}
static void Pixel(GLuint vertex, GLuint fragment, int r, int g) {
  GLuint pipe; glGenProgramPipelines(1, &pipe);
  glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vertex);
  glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, fragment);
  glBindProgramPipeline(pipe);
  glViewport(0, 0, 2, 2); glDrawArrays(GL_TRIANGLES, 0, 3);
  unsigned char px[4]; glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  assert(px[0] == r && px[1] == g && px[2] == 0);
  assert(glGetError() == GL_NO_ERROR);
  glBindProgramPipeline(0); glDeleteProgramPipelines(1, &pipe);
}
int main() {
  // LRU, exact-source distinction, bounds, replacement, failed-entry removal.
  VexaCloneBinaryCache small(12, 2);
  small.Insert("a", 1, {1,2,3}); small.Insert("b", 1, {4,5,6});
  assert(small.Find("a")); small.Insert("c", 1, {7,8,9});
  assert(!small.Find("b") && small.Find("a") && small.SizeBytes() == 8);
  small.Insert("a", 2, {0}); assert(small.Find("a")->Format == 2);
  small.Insert("oversized", 1, std::vector<uint8_t>(20)); assert(small.Size() == 2);
  small.Erase("a"); assert(!small.Find("a"));
  EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY); assert(eglInitialize(d, nullptr, nullptr));
  EGLint attrs[] = {EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_NONE};
  EGLConfig cfg; EGLint n; assert(eglChooseConfig(d,attrs,&cfg,1,&n) && n);
  EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
  EGLContext ctx = eglCreateContext(d,cfg,EGL_NO_CONTEXT,ca);
  EGLint sa[] = {EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};
  EGLSurface surf = eglCreatePbufferSurface(d,cfg,sa); assert(eglMakeCurrent(d,surf,surf,ctx));
  GLuint fragment = Program(GL_FRAGMENT_SHADER,fs);
  GLuint a = Clone(fragment,vs);
  std::string demoted(vs); assert(VexaDemoteOutputs(demoted,{"unusedColor"}));
  const auto* binary = g_vexa_clone_binaries.Find(demoted); assert(binary);
  GLuint b = Clone(fragment,vs); assert(a != b);
  glProgramUniform4f(a,glGetUniformLocation(a,"tint"),1,0,0,1);
  glProgramUniform4f(b,glGetUniformLocation(b,"tint"),0,1,0,1);
  Pixel(a,fragment,255,0); Pixel(b,fragment,0,255); Pixel(a,fragment,255,0);
  std::string changed = std::string(vs) + "\n// distinct source generation\n";
  GLuint c = Clone(fragment,changed.c_str()); assert(g_vexa_clone_binaries.Size() == 2);
  // A rejected binary must compile successfully through the existing fallback.
  g_vexa_clone_binaries.Insert(demoted,0xFFFFFFFFu,{1,2,3,4});
  GLuint recovered = Clone(fragment,vs);
  glProgramUniform4f(recovered,glGetUniformLocation(recovered,"tint"),1,0,0,1);
  Pixel(recovered,fragment,255,0);
  // Compare the two construction paths on this driver, excluding original creation.
  constexpr int repeats = 20;
  double cold = 0, warm = 0;
  for (int i=0;i<repeats;++i) {
    auto start = std::chrono::steady_clock::now();
    GLuint p = VexaLoadCloneBinary(demoted); assert(p); glDeleteProgram(p);
    warm += std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    start = std::chrono::steady_clock::now();
    p = Program(GL_VERTEX_SHADER,demoted.c_str()); glDeleteProgram(p);
    cold += std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  }
  printf("PASS cache hit, independent rendered uniforms, changed source, corrupt fallback, LRU/budget; driver microbenchmark binary=%.3fms source=%.3fms\n",warm/repeats,cold/repeats);
  glDeleteProgram(a); glDeleteProgram(b); glDeleteProgram(c); glDeleteProgram(recovered); glDeleteProgram(fragment);
  eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
  eglDestroySurface(d,surf); eglDestroyContext(d,ctx); eglTerminate(d);
}
'''
out.mkdir(parents=True, exist_ok=True)
cpp = out / "clone_cache_probe.cpp"
cpp.write_text(source)
clang = Path.home() / "AppData/Local/Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe"
subprocess.run([str(clang), "--target=aarch64-linux-android28", "-std=c++17", "-O2", "-static-libstdc++", "-UNDEBUG", "-I", str(fex), str(cpp), "-lEGL", "-lGLESv3", "-o", str(out / "clone_cache_probe")], check=True)
