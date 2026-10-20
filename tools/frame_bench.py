#!/usr/bin/env python3
"""Megabonk frame-pacing bench over adb: SurfaceFlinger present intervals, queueBuffer fps,
per-thread CPU, CPU/GPU clocks, thermal state and the game's Player.log (VK-PERF lines).

usage: frame_bench.py OUT_DIR LABEL {vulkan|gl} [ENV=VALUE ...]
"""
import json, os, pathlib, re, statistics, subprocess, sys, time

os.environ["MSYS_NO_PATHCONV"] = "1"
PKG = 'com.critical.vexaemulator'
ROOT = '/data/user/0/' + PKG + '/files'
VSYNC_MS = 1000 / 144


def adb(*args, check=True):
    r = subprocess.run(['adb', *args], capture_output=True)
    if check and r.returncode:
        raise SystemExit(r.stderr.decode(errors='replace'))
    return r.stdout


def sh(cmd):
    return adb('shell', cmd, check=False).decode(errors='replace')


def main():
    out_dir, label, backend, *extra = sys.argv[1:]
    out = pathlib.Path(out_dir) / label
    out.mkdir(parents=True, exist_ok=True)
    env = ['DISPLAY=:0', 'SDL_VIDEODRIVER=x11', 'SteamAppId=3405340', 'VEXA_GL_SRGB_SURFACE=1', 'VEXA_UNITY_NATIVE_MODE=1',
           'HOME=' + ROOT + '/guests/megabonk', 'VEXA_GRAPHICS_BACKEND=' + backend] + extra
    if not any(e.startswith('VEXA_GLX_SWAP_INTERVAL=') for e in env):
        env.append('VEXA_GLX_SWAP_INTERVAL=0')
    args = ['-screen-fullscreen', '0', '-screen-width', '2712', '-screen-height', '1220',
            '-force-' + ('vulkan' if backend == 'vulkan' else 'glcore')]
    adb('shell', 'am', 'force-stop', PKG)
    time.sleep(2)
    cmd = ['shell', 'am', 'start', '-n', PKG + '/.GameActivity', '--ez', 'directLaunch', 'true']
    for k, v in {'directExecutable': ROOT + '/guests/megabonk/Megabonk.x86_64', 'directRootfs': ROOT + '/rootfs',
                 'directThunkHost': ROOT + '/thunks/host', 'directThunkGuest': ROOT + '/thunks/guest',
                 'directWorkdir': ROOT + '/guests/megabonk', 'directArtifacts': ROOT + '/artifacts/megabonk'}.items():
        cmd += ['--es', k, v]
    cmd += ['--esa', 'directEnv', ','.join(env), '--esa', 'directArgs', ','.join(args)]
    adb(*cmd)
    time.sleep(50)
    pid = sh('pidof -s ' + PKG + ':runtime_worker').strip()
    (out / 'thermal-before.txt').write_text(sh('dumpsys thermalservice | grep -E "Thermal Status|mName=(CPU|GPU)"'))

    layers = sh('dumpsys SurfaceFlinger --list')
    layer = next((l.strip() for l in layers.splitlines() if 'SurfaceView' in l and 'vexaemulator' in l and 'BLAST' in l), None)
    sh('dumpsys SurfaceFlinger --latency-clear')
    with (out / 'logcat.txt').open('wb') as f:
        proc = subprocess.Popen(['adb', 'logcat', '-T', '1', '-v', 'threadtime'], stdout=f, stderr=subprocess.DEVNULL)
        time.sleep(1)
        latency = sh(f'dumpsys SurfaceFlinger --latency "{layer}"') if layer else ''
        (out / 'top-threads.txt').write_text(sh(f'top -H -b -d 2 -n 3 -p {pid} -o TID,PR,NI,%CPU,S,CPU,TIME+,CMD'))
        freqs = []
        for _ in range(4):
            freqs.append(sh('for p in /sys/devices/system/cpu/cpufreq/policy*; do echo $p $(cat $p/scaling_cur_freq) '
                            '$(cat $p/scaling_max_freq); done; cat /proc/gpufreqv2/gpufreq_status 2>/dev/null | head -8;'
                            ' cat /sys/kernel/ged/hal/current_freqency 2>/dev/null; cat /sys/kernel/ged/hal/gpu_utilization 2>/dev/null'))
            time.sleep(1)
        (out / 'freqs.txt').write_text('\n----\n'.join(freqs))
        time.sleep(3)
        proc.terminate(); proc.wait()
    (out / 'sf-latency.txt').write_text(f'{layer}\n{latency}')
    (out / 'Player.log').write_bytes(adb('shell', 'run-as', PKG, 'cat', 'files/guests/megabonk/.config/unity3d/Ved/Megabonk/Player.log'))
    (out / 'screen.png').write_bytes(adb('exec-out', 'screencap', '-p'))
    (out / 'thermal-after.txt').write_text(sh('dumpsys thermalservice | grep -E "Thermal Status|mName=(CPU|GPU)"'))

    fps = [float(m[1]) for line in (out / 'logcat.txt').read_text(errors='replace').splitlines()
           if (m := re.search(r'SurfaceView.*vexa.*queueBuffer: fps=([\d.]+)', line))]
    rows = [list(map(int, l.split())) for l in latency.splitlines()[1:] if len(l.split()) == 3]
    present = [r[1] for r in rows if 0 < r[1] < (1 << 62)]
    deltas = sorted((b - a) / 1e6 for a, b in zip(present, present[1:]))
    pct = lambda p: round(deltas[min(len(deltas) - 1, int(p * len(deltas)))], 2) if deltas else None
    result = {'label': label, 'backend': backend, 'extra_env': extra,
              'queue_fps_mean': round(statistics.mean(fps), 1) if fps else None, 'queue_fps': fps,
              'sf_present': {'n': len(deltas), 'fps': round(1000 * len(deltas) / sum(deltas), 1) if deltas else None,
                             'p50_ms': pct(.5), 'p95_ms': pct(.95), 'p99_ms': pct(.99),
                             'vsyncs_hist': {k: sum(1 for d in deltas if max(1, round(d / VSYNC_MS)) == k) for k in range(1, 7)}}}
    (out / 'summary.json').write_text(json.dumps(result, indent=1))
    print(json.dumps({k: v for k, v in result.items() if k != 'queue_fps'}))


if __name__ == '__main__':
    main()
