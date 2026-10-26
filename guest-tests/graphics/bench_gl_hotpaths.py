"""Build matched native CPU/driver microbenchmarks from HEAD and current GL code.

No actual draw is issued: this isolates upload bookkeeping and pre-draw setup,
not GPU rendering, guest ABI cost, frame time, or gameplay FPS.
"""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
repo=root.parent/'FEX'
fex=repo/'ThunkLibs/libGL'
out=root/'logs/gl-hotpath-20261026'
before=out/'baseline-source';before.mkdir(parents=True,exist_ok=True)
for name in subprocess.check_output(['git','-C',str(repo),'ls-tree','--name-only','HEAD:ThunkLibs/libGL']).decode().splitlines():
    if name.endswith(('.inc','.h')):
        (before/name).write_bytes(subprocess.check_output(['git','-C',str(repo),'show','HEAD:ThunkLibs/libGL/'+name]))
template=Path(__file__).with_name('gl_hotpath_probe.cpp.in').read_text().split('int main(')[0]
main=r'''
int main(){
  EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);assert(eglInitialize(d,nullptr,nullptr));
  EGLint ca[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_NONE};EGLConfig cfg;EGLint n;assert(eglChooseConfig(d,ca,&cfg,1,&n)&&n);
  EGLint attrs[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},sa[]={EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};EGLContext a=eglCreateContext(d,cfg,EGL_NO_CONTEXT,attrs);EGLSurface s=eglCreatePbufferSurface(d,cfg,sa);assert(eglMakeCurrent(d,s,s,a));
  VexaRegisterProgramContext(a,EGL_NO_CONTEXT);VexaSelectProgramContext(a);
  GLuint buf;glGenBuffers(1,&buf);glBindBuffer(GL_ARRAY_BUFFER,buf);unsigned char bytes[1024]{};glBufferData(GL_ARRAY_BUFFER,sizeof(bytes),bytes,GL_DYNAMIC_DRAW);
  auto operation=[&](int i){unsigned char value=i;fexfn_impl_libGL_glBufferSubData(GL_ARRAY_BUFFER,3,1,&value);VexaLegacyArrayScope scope(0,3);assert(scope.Ready);VexaPreDraw();};
  for(int i=0;i<2000;++i)operation(i);
  for(int batch=0;batch<9;++batch){queries=capacityQueries=clocks=0;auto begin=std::chrono::steady_clock::now();for(int i=0;i<10000;++i)operation(i);auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();printf("BATCH ns_per_op=%.2f queries=%u capacity=%u clocks=%u\n",ns/10000.,queries,capacityQueries,clocks);}
  eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,a);eglDestroySurface(d,s);eglTerminate(d);
}
'''
clang=Path.home()/'AppData/Local/Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe'
for label,headers in [('before',before),('after',fex)]:
    host=(fex/'libGL_Host.cpp').read_text() if label=='after' else subprocess.check_output(['git','-C',str(repo),'show','HEAD:ThunkLibs/libGL/libGL_Host.cpp']).decode()
    def part(start,end):
        i=host.index(start);return host[i:host.index(end,i)]
    replacements={
      '@UPLOAD_OPTIONS@':part('// Opt-in diagnostics/experiment' if label=='after' else '// Temporary A/B upload experiment.', 'void GLHostMarkProgress('),
      '@UPLOAD@':part('static void fexfn_impl_libGL_glBufferSubData(', 'static void fexfn_impl_libGL_glBufferStorage('),
      '@UNPACK@':part('struct VexaUnpackScope {', '#if defined(BUILD_ANDROID)\n#include "bcn_gpu_decode.inc"' if label=='after' else 'void VexaBCLogOnce('),
      '@PRECISION@':part('[[maybe_unused]] static size_t VexaShaderPrecisionInsertPos(', '#include "arb_assembly_translate.h"'),
      '@PRE_DRAW@':part('static void VexaPreDraw() {', '// ETS2 keeps a program pipeline bound')}
    source=template
    for key,value in replacements.items():source=source.replace(key,value)
    cpp=out/f'bench-{label}.cpp';cpp.write_text(source+main)
    subprocess.run([str(clang),'--target=aarch64-linux-android28','-std=c++20','-O2','-static-libstdc++','-UNDEBUG','-I',str(headers),str(cpp),'-lEGL','-lGLESv3','-o',str(out/f'bench-{label}')],check=True)
