#!/usr/bin/env python3
"""Record a Perfetto trace of the running game (MediaTek GED GPU DVFS, gpu_frequency/work_period,
SurfaceFlinger frame timeline, scheduler) and summarize GPU clock, loading and present cadence.

usage: gpu_trace.py OUT.pftrace [SECONDS]
"""
import os, pathlib, statistics, subprocess, sys

os.environ["MSYS_NO_PATHCONV"] = "1"
CONFIG = """
buffers { size_kb: 65536 fill_policy: RING_BUFFER }
data_sources { config { name: "linux.ftrace" ftrace_config {
  ftrace_events: "ged/*"
  ftrace_events: "power/gpu_frequency"
  ftrace_events: "power/gpu_work_period"
  ftrace_events: "power/cpu_frequency"
  ftrace_events: "sched/sched_switch"
  ftrace_events: "sched/sched_waking"
  atrace_categories: "gfx"
  atrace_categories: "view"
  atrace_apps: "com.critical.vexaemulator:runtime_worker"
} } }
data_sources { config { name: "android.surfaceflinger.frametimeline" } }
data_sources { config { name: "linux.process_stats" process_stats_config { scan_all_processes_on_start: true } } }
duration_ms: %d
"""


def main():
    out = pathlib.Path(sys.argv[1])
    seconds = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    remote = '/data/misc/perfetto-traces/vexa_gpu.pftrace'
    subprocess.run(['adb', 'shell', 'perfetto', '--txt', '-c', '-', '-o', remote],
                   input=(CONFIG % (seconds * 1000)).encode(), check=True, capture_output=True)
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['adb', 'pull', remote, str(out)], check=True, capture_output=True)
    summarize(out)


def summarize(path):
    from perfetto.trace_processor import TraceProcessor
    tp = TraceProcessor(trace=str(path))
    q = lambda sql: list(tp.query(sql))
    print('== counter tracks')
    for r in q("select t.name, count(*) n, min(c.value) mn, avg(c.value) av, max(c.value) mx from counter c "
               "join counter_track t on c.track_id=t.id where t.name like '%gpu%' or t.name like '%GPU%' or t.name like '%ged%' "
               "or t.name like '%Freq%' or t.name like '%Loading%' group by t.name order by n desc limit 40"):
        print(f'  {r.name:60} n={r.n:6} min={r.mn:.1f} avg={r.av:.1f} max={r.mx:.1f}')
    print('== frame timeline (vexa layer)')
    rows = q("select a.ts, a.dur, a.present_type, a.jank_type, a.layer_name from actual_frame_timeline_slice a "
             "where a.layer_name like '%vexaemulator%' order by a.ts")
    if rows:
        ts = [r.ts for r in rows]
        deltas = [(b - a) / 1e6 for a, b in zip(ts, ts[1:])]
        print(f'  frames={len(rows)} fps={1000 * len(deltas) / sum(deltas):.1f} interval_p50={statistics.median(deltas):.2f}ms')
        kinds = {}
        for r in rows:
            kinds[r.jank_type] = kinds.get(r.jank_type, 0) + 1
        print('  jank types:', kinds)
    print('== ged/raw ftrace event names')
    for r in q("select name, count(*) n from ftrace_event where name like 'GPU_DVFS%' or name like 'gpu%' group by name order by n desc limit 30"):
        print(f'  {r.name:60} {r.n}')
    tp.close()


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--summarize':
        summarize(pathlib.Path(sys.argv[2]))
    else:
        main()
