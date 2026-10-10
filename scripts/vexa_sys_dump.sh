#!/system/bin/sh
# vexa_sys_dump.sh <out_dir> <package>
# Runs ON DEVICE under `su`. Dumps system-level context that matters while
# debugging VEXA runs: thermal/throttling state, cpufreq, devfreq (GPU),
# memory pressure, cpuset layout, selinux avc stats, and app-facing dumpsys.

out="$1"
pkg="${2:-com.critical.vexaemulator}"
mkdir -p "$out" 2>/dev/null

{
  echo "=== thermal zones ==="
  for z in /sys/class/thermal/thermal_zone*; do
    echo "$z type=$(cat "$z/type" 2>/dev/null) temp=$(cat "$z/temp" 2>/dev/null)"
  done
  echo "=== cooling devices ==="
  for c in /sys/class/thermal/cooling_device*; do
    echo "$c type=$(cat "$c/type" 2>/dev/null) cur=$(cat "$c/cur_state" 2>/dev/null) max=$(cat "$c/max_state" 2>/dev/null)"
  done
  echo "=== cpufreq policies ==="
  for p in /sys/devices/system/cpu/cpufreq/policy*/; do
    echo "$p cpus=$(cat "$p/related_cpus" 2>/dev/null) cur=$(cat "$p/scaling_cur_freq" 2>/dev/null) gov=$(cat "$p/scaling_governor" 2>/dev/null) max=$(cat "$p/cpuinfo_max_freq" 2>/dev/null)"
  done
  echo "=== devfreq ==="
  for d in /sys/class/devfreq/*/; do
    echo "$d cur=$(cat "$d/cur_freq" 2>/dev/null) gov=$(cat "$d/governor" 2>/dev/null)"
  done
  echo "=== PSI (pressure stall) ==="
  for p in cpu memory io; do
    echo "-- $p --"; cat "/proc/pressure/$p" 2>/dev/null
  done
  echo "=== cpuset groups ==="
  for c in /dev/cpuset/*/cpus /dev/cpuset/cpus; do
    [ -f "$c" ] && echo "$c = $(cat "$c" 2>/dev/null)"
  done
  echo "=== selinux avc cache ==="
  head -15 /sys/fs/selinux/avc/cache_stats 2>/dev/null
  echo "=== vmstat highlights ==="
  grep -E 'pgmajfault|pswpin|pswpout|oom_kill|allocstall' /proc/vmstat 2>/dev/null | head -15
  echo "=== buddyinfo ==="
  cat /proc/buddyinfo 2>/dev/null
  echo "=== pstore ==="
  ls -la /sys/fs/pstore/ 2>/dev/null
  echo "=== last_kmsg ==="
  head -60 /proc/last_kmsg 2>/dev/null
  echo "=== dumpsys thermalservice ==="
  dumpsys thermalservice 2>/dev/null | head -60
  echo "=== dumpsys battery ==="
  dumpsys battery 2>/dev/null | head -20
  echo "=== dumpsys gfxinfo $pkg ==="
  dumpsys gfxinfo "$pkg" 2>/dev/null | head -60
  echo "=== dumpsys meminfo $pkg ==="
  dumpsys meminfo "$pkg" 2>/dev/null | head -80
  echo "=== dropbox mentions ($pkg) ==="
  dumpsys dropbox 2>/dev/null | grep -i "$pkg" | head -20
  echo "=== /data/anr ==="
  ls -la /data/anr/ 2>/dev/null | tail -15
} > "$out/system.txt" 2>&1

echo "done sys"
