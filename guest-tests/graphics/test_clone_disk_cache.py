"""Run rendered cold/warm/corrupt-cache probes in separate Android processes."""
from pathlib import Path
import argparse
import subprocess
import time

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--out', type=Path, default=root / 'logs/clone-disk-cache')
args = parser.parse_args()
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
subprocess.run(['python', str(Path(__file__).with_name('test_program_cache_lifecycle.py')), '--out', str(out)], check=True)
remote = '/data/local/tmp/vexa-cache-regression-' + str(time.time_ns())
subprocess.run(['adb','shell','mkdir','-p',remote],check=True)
subprocess.run(['adb','push',str(out/'program_cache_probe'),remote+'/probe'],check=True)
subprocess.run(['adb','shell','chmod','700',remote+'/probe'],check=True)

def run(label, *env):
    result = subprocess.run(['adb','shell','env','VEXA_GL_PROGRAM_CACHE_DIR='+remote,*env,remote+'/probe'],capture_output=True,text=True,check=True)
    text = result.stdout + result.stderr
    (out/(label+'.log')).write_text(text,encoding='utf-8')
    assert 'PASS rendered' in text, text
    print(label, text.splitlines()[0],flush=True)
    return text

cold = run('cold')
assert 'cache=miss' in cold
warm = run('warm')
assert 'cache=disk' in warm and 'cache=miss' not in warm
names = subprocess.check_output(['adb','shell','ls',remote],text=True).split()
files = [n for n in names if n.startswith('clone-') and n.endswith('.bin')]
assert len(files) == 3

# A malformed/truncated file must be ignored and replaced through source
# compilation. These are private files in this test's uniquely named folder.
for name in files:
    subprocess.run(['adb','shell','sh','-c',"'printf corrupt > "+remote+'/'+name+"'"],check=True)
corrupt = run('corrupt')
assert 'cache=miss' in corrupt
recovered = run('recovered')
assert 'cache=disk' in recovered and 'cache=miss' not in recovered

# Keep the complete payload intact but change the binary format so that the
# driver rejects it, rather than the file parser. Source fallback must work.
for name in files:
    path = out/name
    subprocess.run(['adb','pull',remote+'/'+name,str(path)],check=True,capture_output=True)
    data = bytearray(path.read_bytes())
    data[4:8] = (0xffffffff).to_bytes(4,'little')
    path.write_bytes(data)
    subprocess.run(['adb','push',str(path),remote+'/'+name],check=True,capture_output=True)
rejected = run('driver-rejected')
assert 'cache=miss' in rejected
assert 'cache=disk' in run('driver-recovered')
disabled = run('disabled','VEXA_GL_CLONE_CACHE=0')
assert 'binary_hit=1' not in disabled and 'cache=miss' in disabled
assert 'PASS disk quota' in run('quota','VEXA_TEST_CACHE_QUOTA=1')
print('PASS separate-process disk hits, rendered pixels, corrupt-file recovery, driver rejection fallback; artifacts:',out)
