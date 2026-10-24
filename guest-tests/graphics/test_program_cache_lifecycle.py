"""Compile the actual Android GL cache/lifecycle code into a rendered probe.

Run the resulting executable with adb shell. No FEX or game launch is needed.
"""
from pathlib import Path
import argparse
import subprocess

root = Path(__file__).resolve().parents[2]
fex = root.parent / "FEX/ThunkLibs/libGL"
parser = argparse.ArgumentParser()
parser.add_argument("--out", type=Path, default=root / "logs/program-cache-lifecycle")
args = parser.parse_args()
out = args.out.resolve()
host = (fex / "libGL_Host.cpp").read_text()

def section(start, end):
    return host[host.index(start):host.index(end, host.index(start))]

code = r'''
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include "clone_binary_cache.h"
#include "program_cache_state.inc"
'''
code += section("// ---- separable-pipeline vertex clone", "// Validates each bound pipeline once.")
code += section("static void VexaPreDraw() {", "// ETS2 keeps a program pipeline bound")
code += section("static void VexaGL_glUseProgramStages(", "static void VexaGL_glProgramUniform1i(")
code += section("GLuint fexfn_impl_libGL_glCreateProgram() {", "GLuint fexfn_impl_libGL_glCreateShader(GLenum a_0) {")
code += r'''
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
static GLuint Compile(GLenum stage, const std::string& source) {
  GLuint shader = glCreateShader(stage);
  const char* p = source.c_str(); glShaderSource(shader, 1, &p, nullptr);
  glCompileShader(shader); GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok); assert(ok);
  VexaSnapshotCompiledShader(shader);
  return shader;
}
static GLuint Link(GLuint shader, GLuint program = 0) {
  if (!program) program = fexfn_impl_libGL_glCreateProgram();
  glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);
  glAttachShader(program, shader); glLinkProgram(program);
  GLint ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok); assert(ok);
  VEXA_PROGRAM_LOCK(); VexaSnapshotProgramSources(program); VexaRefreshProgramGeneration();
  return program;
}
static GLuint Pipe(GLuint vertex, GLuint fragment) {
  GLuint p; glGenProgramPipelines(1, &p);
  VexaGL_glUseProgramStages(p, GL_VERTEX_SHADER_BIT, vertex);
  VexaGL_glUseProgramStages(p, GL_FRAGMENT_SHADER_BIT, fragment);
  return p;
}
static void Pixel(GLuint pipe, int r, int g, int b = 0) {
  VexaGL_glBindProgramPipeline(pipe);
  glViewport(0, 0, 2, 2); glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  unsigned char px[4]; glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  if (px[0] != r || px[1] != g || px[2] != b)
    fprintf(stderr, "pixel expected=%d,%d,%d actual=%d,%d,%d\n",r,g,b,px[0],px[1],px[2]);
  assert(px[0] == r && px[1] == g && px[2] == b);
  assert(glGetError() == GL_NO_ERROR);
}
int main() {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY); assert(eglInitialize(display,nullptr,nullptr));
  EGLint config_attrs[] = {EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_NONE};
  EGLConfig config; EGLint count; assert(eglChooseConfig(display,config_attrs,&config,1,&count) && count);
  EGLint context_attrs[] = {EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
  EGLint surface_attrs[] = {EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};
  EGLContext ctx = eglCreateContext(display,config,EGL_NO_CONTEXT,context_attrs);
  EGLSurface surf = eglCreatePbufferSurface(display,config,surface_attrs);
  assert(eglMakeCurrent(display,surf,surf,ctx));
  VexaRegisterProgramContext(ctx,EGL_NO_CONTEXT); VexaSelectProgramContext(ctx);
  GLuint vshader = Compile(GL_VERTEX_SHADER,vs), fshader = Compile(GL_FRAGMENT_SHADER,fs);
  GLuint a = Link(vshader), b = Link(vshader), fragment = Link(fshader);
  assert(g_vexa_prog_sources[a].size() == 1 && g_vexa_prog_sources[b].size() == 1);
  assert(g_vexa_prog_sources[a][0].second == g_vexa_prog_sources[b][0].second);
  // glShaderSource alone must not replace the successfully compiled code
  // generation that can be linked into another program.
  const char* uncompiled_source = "not a compiled shader";
  glShaderSource(vshader,1,&uncompiled_source,nullptr);
  VexaSnapshotProgramSources(a);
  assert(g_vexa_prog_sources[a][0].second.find("not a compiled shader") == std::string::npos);
  // A delete-pending shader remains queryable while attached to a program.
  fexfn_impl_libGL_glDeleteShader(vshader); assert(glIsShader(vshader));
  glLinkProgram(b); VexaSnapshotProgramSources(b); VexaRefreshProgramGeneration();
  assert(g_vexa_prog_sources[b].size() == 1);
  glProgramUniform4f(a,glGetUniformLocation(a,"tint"),1,0,0,1);
  glProgramUniform4f(b,glGetUniformLocation(b,"tint"),0,1,0,1);
  GLuint pa = Pipe(a,fragment), pb = Pipe(b,fragment);
  GLuint ca = VexaBuildStrippedVertexProgram(pa); assert(ca);
  GLuint cb = VexaBuildStrippedVertexProgram(pb); assert(cb && cb != ca);
  // Original values must be copied on BOTH source compile and binary hit.
  Pixel(pa,255,0); Pixel(pb,0,255);
  assert(VexaProgramContext().Shared->Clones[a].size() == 1);
  GLuint unrelated = Link(Compile(GL_FRAGMENT_SHADER,fs));
  assert(VexaProgramContext().OwnedClones.size() == 2);
  Pixel(pa,255,0); Pixel(pb,0,255);
  fexfn_impl_libGL_glDeleteProgram(unrelated);
  // Replacing the shader and relinking must replace the source generation.
  glDetachShader(a,vshader);
  std::string changed(vs); changed.replace(changed.find("color = tint;"),13,"color = tint.bgra;");
  GLuint newer = Compile(GL_VERTEX_SHADER,changed);
  Link(newer,a);
  assert(g_vexa_prog_sources[a].size() == 1);
  assert(g_vexa_prog_sources[a][0].second.find("tint.bgra") != std::string::npos);
  assert(VexaProgramContext().OwnedClones.empty());
  glProgramUniform4f(a,glGetUniformLocation(a,"tint"),1,0,0,1);
  ca = VexaBuildStrippedVertexProgram(pa); assert(ca); Pixel(pa,0,0,255);
  // Cloning must preserve the original's delete-pending lifetime.
  fexfn_impl_libGL_glDeleteProgram(a); assert(glIsProgram(a));
  GLint delete_status = GL_FALSE;
  fexfn_impl_libGL_glGetProgramiv(a, GL_DELETE_STATUS, &delete_status); assert(delete_status);
  assert(g_vexa_prog_sources.contains(a)); Pixel(pa,0,0,255);
  VexaGL_glBindProgramPipeline(0);
  fexfn_impl_libGL_glDeleteProgramPipelines(1,&pa);
  assert(!glIsProgram(a)); assert(!g_vexa_prog_sources.contains(a));
  assert(!VexaProgramContext().Shared->Locations.contains(ca));
  fexfn_impl_libGL_glDeleteProgramPipelines(1,&pb);
  const char* typed_vs = R"(#version 310 es
precision highp float;
precision highp int;
uniform mat4 model;
uniform vec4 palette[2];
uniform ivec2 selection;
uniform uvec2 indices;
out vec4 color;
out vec4 unusedColor;
void main() {
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  gl_Position = model * vec4(p * 2.0 - 1.0, 0.0, 1.0);
  color = palette[selection.x & 1] * float(indices.y) / 4.0;
  unusedColor = vec4(1.0);
}
)";
  GLuint typed = Link(Compile(GL_VERTEX_SHADER,typed_vs));
  const float identity[] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  const float palette[] = {1,0,0,1, 0,1,0,1};
  glProgramUniformMatrix4fv(typed,glGetUniformLocation(typed,"model"),1,GL_FALSE,identity);
  glProgramUniform4fv(typed,glGetUniformLocation(typed,"palette[0]"),2,palette);
  glProgramUniform2i(typed,glGetUniformLocation(typed,"selection"),1,2);
  glProgramUniform2ui(typed,glGetUniformLocation(typed,"indices"),3,4);
  GLuint pt = Pipe(typed,fragment); assert(VexaBuildStrippedVertexProgram(pt)); Pixel(pt,0,255);
  fexfn_impl_libGL_glDeleteProgramPipelines(1,&pt); fexfn_impl_libGL_glDeleteProgram(typed);
  // Shared context sees program snapshots; pipeline bindings stay local.
  auto original_state = g_vexa_program_context;
  EGLContext shared = eglCreateContext(display,config,ctx,context_attrs);
  VexaRegisterProgramContext(shared,ctx);
  assert(eglMakeCurrent(display,surf,surf,shared)); VexaSelectProgramContext(shared);
  assert(g_vexa_program_context->Shared == original_state->Shared);
  assert(g_vexa_prog_sources.contains(b)); assert(!g_vexa_intended_pipe);
  GLuint ps = Pipe(b,fragment); assert(VexaBuildStrippedVertexProgram(ps)); Pixel(ps,0,255);
  // Independent context may reuse numerical GL IDs but never these sources.
  EGLContext independent = eglCreateContext(display,config,EGL_NO_CONTEXT,context_attrs);
  VexaRegisterProgramContext(independent,EGL_NO_CONTEXT);
  assert(eglMakeCurrent(display,surf,surf,independent)); VexaSelectProgramContext(independent);
  assert(g_vexa_prog_sources.empty() && g_vexa_clones_of_prog.empty());
  GLuint own = Link(Compile(GL_VERTEX_SHADER,changed));
  assert(g_vexa_prog_sources[own][0].second.find("tint.bgra") != std::string::npos);
  assert(eglMakeCurrent(display,surf,surf,shared)); VexaSelectProgramContext(shared);
  assert(g_vexa_prog_sources[b][0].second.find("tint.bgra") == std::string::npos);
  Pixel(ps,0,255);
  // EGL destruction on another thread is deferred until the current owner
  // releases it. Cache metadata must follow that same lifetime.
  EGLContext worker_context = eglCreateContext(display,config,ctx,context_attrs);
  EGLSurface worker_surface = eglCreatePbufferSurface(display,config,surface_attrs);
  VexaRegisterProgramContext(worker_context,ctx);
  glFinish();
  std::atomic<int> phase {0};
  std::thread worker([&] {
    assert(eglMakeCurrent(display,worker_surface,worker_surface,worker_context));
    VexaSelectProgramContext(worker_context);
    GLuint pipe = Pipe(b,fragment); assert(VexaBuildStrippedVertexProgram(pipe)); Pixel(pipe,0,255);
    phase.store(1);
    while (phase.load()!=2) std::this_thread::yield();
    Pixel(pipe,0,255);
    assert(eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT));
    VexaSelectProgramContext(EGL_NO_CONTEXT);
  });
  while (phase.load()!=1) std::this_thread::yield();
  VexaUnregisterProgramContext(worker_context);
  assert(eglDestroyContext(display,worker_context));
  phase.store(2); worker.join();
  eglDestroySurface(display,worker_surface);
  VexaRefreshProgramGeneration();
  assert(VexaProgramContext().Shared->Retired.empty());
  fexfn_impl_libGL_glDeleteProgramPipelines(1,&ps);
  assert(VexaProgramContext().Shared->Clones.empty());
  assert(VexaProgramContext().Shared->Locations.empty());
  fexfn_impl_libGL_glDeleteProgram(b); fexfn_impl_libGL_glDeleteProgram(fragment);
  assert(glGetError() == GL_NO_ERROR);
  VexaUnregisterProgramContext(shared); VexaUnregisterProgramContext(independent); VexaUnregisterProgramContext(ctx);
  eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
  eglDestroyContext(display,shared); eglDestroyContext(display,independent); eglDestroyContext(display,ctx);
  eglDestroySurface(display,surf); eglTerminate(display);
  puts("PASS rendered uniform preservation, binary reuse, shared shader, relink generation, deferred deletion, pipeline cleanup, shared/independent contexts");
  if (std::getenv("VEXA_TEST_CACHE_QUOTA")) {
    assert(std::getenv("VEXA_GL_PROGRAM_CACHE_DIR"));
    const auto directory = VexaCloneDiskDirectory(); assert(!directory.empty());
    for (int n = 0; n < 160; ++n) {
      char name[40]; std::snprintf(name,sizeof(name),"/clone-%016llx.bin",static_cast<unsigned long long>(n));
      FILE* file = std::fopen((directory+name).c_str(),"wb"); assert(file);
      assert(ftruncate(fileno(file), n < 16 ? 3 * 1024 * 1024 : 1) == 0); std::fclose(file);
    }
    VexaPruneCloneDisk(directory);
    DIR* dir = opendir(directory.c_str()); assert(dir);
    size_t count = 0; uint64_t bytes = 0;
    while (auto* entry = readdir(dir)) {
      std::string_view name(entry->d_name);
      if (name.size()!=26 || name.substr(0,6)!="clone-" || name.substr(22)!=".bin") continue;
      struct stat st {}; assert(stat((directory+"/"+entry->d_name).c_str(),&st)==0);
      ++count; bytes += st.st_size;
    }
    closedir(dir); assert(count <= 128 && bytes <= 32 * 1024 * 1024);
    puts("PASS disk quota: at most 128 files and 32 MiB");
  }
}
'''
out.mkdir(parents=True, exist_ok=True)
cpp = out / "program_cache_probe.cpp"
cpp.write_text(code)
clang = Path.home() / "AppData/Local/Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe"
subprocess.run([str(clang), "--target=aarch64-linux-android28", "-std=c++20", "-O2", "-static-libstdc++", "-UNDEBUG", "-I", str(fex), str(cpp), "-lEGL", "-lGLESv3", "-o", str(out / "program_cache_probe")], check=True)
