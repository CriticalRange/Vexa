"""Build a real-Mali probe from the production GL hot paths (no FEX launch)."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
fex = root.parent / 'FEX/ThunkLibs/libGL'
host = (fex / 'libGL_Host.cpp').read_text()
def section(begin, end):
    start = host.index(begin)
    return host[start:host.index(end, start)]

source = Path(__file__).with_name('gl_hotpath_probe.cpp.in').read_text()
source = source.replace('@UPLOAD_OPTIONS@', section('// Opt-in diagnostics/experiment', 'void GLHostMarkProgress('))
source = source.replace('@UPLOAD@', section('static void fexfn_impl_libGL_glBufferSubData(', 'static void fexfn_impl_libGL_glBufferStorage('))
source = source.replace('@PRECISION@', section('[[maybe_unused]] static size_t VexaShaderPrecisionInsertPos(', '#include "arb_assembly_translate.h"'))
source = source.replace('@PRE_DRAW@', section('static void VexaPreDraw() {', '// ETS2 keeps a program pipeline bound'))
source = source.replace('@UNPACK@', section('struct VexaUnpackScope {', '#if defined(BUILD_ANDROID)\n#include "bcn_gpu_decode.inc"'))
out = root / 'logs/gl-hotpath-20261026'
out.mkdir(parents=True, exist_ok=True)
cpp = out / 'gl_hotpath_probe.cpp'
cpp.write_text(source)
clang = Path.home() / 'AppData/Local/Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe'
subprocess.run([str(clang), '--target=aarch64-linux-android28', '-std=c++20', '-O2', '-static-libstdc++', '-UNDEBUG', '-I', str(fex), str(cpp), '-lEGL', '-lGLESv3', '-o', str(out / 'gl_hotpath_probe')], check=True)
