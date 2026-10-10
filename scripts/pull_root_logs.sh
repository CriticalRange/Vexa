#!/usr/bin/env bash
set -euo pipefail
# Git Bash on Windows rewrites leading-slash args into C:\Program Files\Git\...
export MSYS_NO_PATHCONV=1

# Usage:
#   ./scripts/pull_root_logs.sh [package] [--watch [seconds]] [--no-debuggerd]
#                                       [--perf seconds] [--dump-jit]
#   ./scripts/pull_root_logs.sh com.critical.vexaemulator
#   ./scripts/pull_root_logs.sh com.critical.vexaemulator --watch 30 --perf 10
#   ./scripts/pull_root_logs.sh --enable-root | --disable-root
#
# Collects root-only diagnostics via `adb shell su`:
#   - device snapshot (real hardware/fingerprint, kernel, selinux)
#   - system context (thermal/throttling, cpufreq, devfreq GPU, PSI, cpuset,
#     selinux avc, dumpsys thermalservice/gfxinfo/meminfo, pstore, last_kmsg)
#   - per-process /proc dumps: status, maps+smaps, fds+dmabuf fdinfo, net/*,
#     mountinfo, auxv/environ, per-task kernel stacks+cpu, debuggerd -b
#   - dmesg (full tail + avc/vexa/segfault filtered)
#   - tombstones + ANR traces matching the package
#   - logcat (pid-filtered + VEXA tag + crash buffer)
#   - app files listing + files/artifacts pull via root
#
# --watch N streams `dmesg -w` and logcat to files for N seconds and takes
# periodic debuggerd backtraces of the :runtime_worker process.
# --perf N  runs `simpleperf stat` for N s on the worker and pulls perf.data
#           from `simpleperf record -g` (best effort).
# --dump-jit additionally copies rwxp memory regions from /proc/<pid>/mem
#           into proc_<pid>/jit/ for offline disassembly.
#
# Requires: rooted device where `adb shell su -c id` works (KernelSU/Magisk).

PKG="com.critical.vexaemulator"
WATCH_SECS=0
DEBUGGERD=1
PERF_SECS=0
DUMP_JIT=0
POSITIONAL=()

while (($#)); do
  case "$1" in
    --watch)
      WATCH_SECS="${2:-30}"
      if [[ "${2:-}" =~ ^[0-9]+$ ]]; then shift 2; else shift; fi
      ;;
    --perf)
      PERF_SECS="${2:-10}"
      if [[ "${2:-}" =~ ^[0-9]+$ ]]; then shift 2; else shift; fi
      ;;
    --dump-jit)
      DUMP_JIT=1
      shift
      ;;
    --no-debuggerd)
      DEBUGGERD=0
      shift
      ;;
    --enable-root)
      adb shell run-as com.critical.vexaemulator touch files/root_logs_enabled \
        && echo "[pull-root-logs] in-app root logging enabled (next launch)" || \
        echo "[pull-root-logs] failed: run-as needs a debuggable install"
      exit 0
      ;;
    --disable-root)
      adb shell run-as com.critical.vexaemulator rm -f files/root_logs_enabled \
        && echo "[pull-root-logs] in-app root logging disabled" || true
      exit 0
      ;;
    -h|--help)
      sed -n '2,20p' "${BASH_SOURCE[0]}"
      exit 0
      ;;
    -*)
      echo "[pull-root-logs] unknown option: $1" >&2
      exit 2
      ;;
    *)
      POSITIONAL+=("$1")
      shift
      ;;
  esac
done

PKG="${POSITIONAL[0]:-$PKG}"

ADB_SERIAL_ARG=()
if [[ -n "${ADB_SERIAL:-}" ]]; then
  ADB_SERIAL_ARG=(-s "${ADB_SERIAL}")
fi
ADB=(adb ${ADB_SERIAL_ARG[@]+"${ADB_SERIAL_ARG[@]}"})

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TS="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${PROJECT_DIR}/logs/${TS}/root"
mkdir -p "${OUT_DIR}"

echo "[pull-root-logs] package=${PKG}"
echo "[pull-root-logs] local=${OUT_DIR}"

# adb joins argv with spaces, so `su -c` must receive one quoted string. Wrap in
# double quotes (inner single quotes survive) and re-escape '$' so the device
# shell doesn't expand our variables before su sees them.
su_run() {
  local cmd="${1//\$/\\\$}"
  "${ADB[@]}" shell "su -c \"$cmd\"" 2>/dev/null || true
}
su_pull() { "${ADB[@]}" exec-out "su -c \"cat '$1'\"" > "$2" 2>/dev/null || true; }

# --- sanity: root works? ---------------------------------------------------
if ! su_run "id" | grep -q "uid=0"; then
  echo "[pull-root-logs] ERROR: 'adb shell su -c id' did not return uid=0." >&2
  echo "[pull-root-logs] Is the device rooted and is the shell user authorized?" >&2
  exit 1
fi

vexa_pids() {
  su_run "for p in '${PKG}' '${PKG}:runtime' '${PKG}:runtime_worker'; do pidof -s \$p; done" \
    | tr -d '\r' | sed '/^$/d' | sort -un
}

# --- device snapshot --------------------------------------------------------
{
  echo "=== device snapshot ${TS} ==="
  echo "--- fingerprint (may be spoofed) ---"
  su_run "getprop ro.build.fingerprint"
  echo "--- boot-level identity (usually not spoofed) ---"
  for p in ro.boot.bootloader ro.boot.em.model ro.board.platform \
           ro.hardware ro.hardware.chipname ro.soc.model ro.build.tags; do
    echo "$p = $(su_run "getprop $p")"
  done
  echo "--- kernel ---"
  su_run "uname -a"
  echo "--- selinux ---"
  su_run "getenforce"
  echo "--- uptime ---"
  su_run "cat /proc/uptime"
  echo "--- meminfo (head) ---"
  su_run "head -20 /proc/meminfo"
  echo "--- cpu ---"
  su_run "grep -m1 'Features' /proc/cpuinfo; grep -c '^processor' /proc/cpuinfo"
  echo "--- gpu ---"
  su_run "getprop ro.hardware.vulkan; getprop ro.hardware.egl; getprop ro.gfx.driver.1"
} > "${OUT_DIR}/device.txt"
echo "[pull-root-logs] device.txt done"

# --- process inventory ------------------------------------------------------
PIDS="$(vexa_pids)"
{
  echo "=== vexa processes ==="
  su_run "ps -AZ | grep -i vexa || ps -A | grep -i vexa"
  echo "--- pids: ${PIDS:-none} ---"
} > "${OUT_DIR}/processes.txt"

REMOTE_TMP="/data/local/tmp/vexa_root_logs"
"${ADB[@]}" shell "mkdir -p ${REMOTE_TMP}"
"${ADB[@]}" push "$(cygpath -w "${SCRIPT_DIR}/vexa_proc_dump.sh")" "${REMOTE_TMP}/vexa_proc_dump.sh" >/dev/null
"${ADB[@]}" push "$(cygpath -w "${SCRIPT_DIR}/vexa_sys_dump.sh")" "${REMOTE_TMP}/vexa_sys_dump.sh" >/dev/null
su_run "chmod 755 ${REMOTE_TMP}/vexa_proc_dump.sh ${REMOTE_TMP}/vexa_sys_dump.sh"

if [[ -z "${PIDS}" ]]; then
  echo "[pull-root-logs] no ${PKG} processes running (proc dumps skipped)"
else
  JIT_FLAG=""
  ((DUMP_JIT == 1)) && JIT_FLAG="jit"
  for pid in ${PIDS}; do
    su_run "sh ${REMOTE_TMP}/vexa_proc_dump.sh ${pid} ${REMOTE_TMP}/proc_${pid} ${JIT_FLAG}" >/dev/null
    d="${OUT_DIR}/proc_${pid}"
    mkdir -p "${d}"
    for f in $(su_run "find ${REMOTE_TMP}/proc_${pid} -type f" | tr -d '\r'); do
      rel="${f#${REMOTE_TMP}/proc_${pid}/}"
      mkdir -p "${d}/$(dirname "${rel}")"
      su_pull "${f}" "${d}/${rel}"
    done
    ((DEBUGGERD == 0)) && rm -f "${d}/debuggerd_backtrace.txt"
    echo "[pull-root-logs] proc dump ${pid} captured"
  done
fi

# --- system context -----------------------------------------------------------
su_run "sh ${REMOTE_TMP}/vexa_sys_dump.sh ${REMOTE_TMP} '${PKG}'" >/dev/null
su_pull "${REMOTE_TMP}/system.txt" "${OUT_DIR}/system.txt"
echo "[pull-root-logs] system.txt done"

# --- perf sampling (optional) -------------------------------------------------
if ((PERF_SECS > 0)); then
  wpid="$(su_run "pidof -s '${PKG}:runtime_worker'" | tr -d '\r')"
  [[ -z "${wpid}" ]] && wpid="$(echo "${PIDS}" | tail -1)"
  if [[ -n "${wpid}" ]]; then
    echo "[pull-root-logs] perf: sampling pid ${wpid} for ${PERF_SECS}s ..."
    su_run "simpleperf stat -p ${wpid} --duration ${PERF_SECS} \
      -e cpu-cycles,instructions,cache-misses,cache-references,branch-misses" \
      > "${OUT_DIR}/perf_stat_${wpid}.txt"
    REMOTE_PERF="${REMOTE_TMP}/perf_${wpid}.data"
    su_run "simpleperf record -p ${wpid} -g --duration ${PERF_SECS} -o ${REMOTE_PERF} >/dev/null 2>&1"
    su_pull "${REMOTE_PERF}" "${OUT_DIR}/perf_${wpid}.data"
    echo "[pull-root-logs] perf captured -> perf_stat_${wpid}.txt + perf_${wpid}.data"
  else
    echo "[pull-root-logs] perf: no vexa pid alive, skipped"
  fi
fi

# --- dmesg ------------------------------------------------------------------
su_run "dmesg" > "${OUT_DIR}/dmesg_full.txt"
grep -iE 'avc|vexa|fex|segfault|sigsegv|oom|audit|binder.*transaction' \
  "${OUT_DIR}/dmesg_full.txt" > "${OUT_DIR}/dmesg_filtered.txt" || true
echo "[pull-root-logs] dmesg captured ($(wc -l < "${OUT_DIR}/dmesg_filtered.txt") filtered lines)"

# --- tombstones -------------------------------------------------------------
{
  echo "=== tombstones ==="
  su_run "ls -la /data/tombstones/ | tail -20"
} > "${OUT_DIR}/tombstones.txt"
mkdir -p "${OUT_DIR}/tombstones"
VEXA_TOMBS="$(su_run "grep -l '${PKG}' /data/tombstones/tombstone_* 2>/dev/null" | tr -d '\r')"
for t in ${VEXA_TOMBS}; do
  name="$(basename "$t")"
  su_pull "$t" "${OUT_DIR}/tombstones/${name}"
  echo "[pull-root-logs] pulled ${name} (mentions ${PKG})"
done
if [[ -z "${VEXA_TOMBS}" ]]; then
  NEWEST="$(su_run "ls -t /data/tombstones/tombstone_* 2>/dev/null | grep -v '\\.pb$' | head -1" | tr -d '\r')"
  if [[ -n "${NEWEST}" ]]; then
    su_pull "${NEWEST}" "${OUT_DIR}/tombstones/$(basename "${NEWEST}")"
    echo "[pull-root-logs] no vexa tombstone; pulled newest (${NEWEST}) for reference"
  fi
fi

# --- ANR traces ---------------------------------------------------------------
mkdir -p "${OUT_DIR}/anr"
ANR_HITS="$(su_run "grep -l '${PKG}' /data/anr/anr_* /data/anr/traces.txt 2>/dev/null" | tr -d '\r')"
for t in ${ANR_HITS}; do
  su_pull "$t" "${OUT_DIR}/anr/$(basename "$t")"
  echo "[pull-root-logs] pulled ANR $(basename "$t") (mentions ${PKG})"
done

# --- logcat -----------------------------------------------------------------
for pid in ${PIDS}; do
  "${ADB[@]}" logcat -d -b all --pid="${pid}" > "${OUT_DIR}/logcat_${pid}.txt" 2>/dev/null || true
done
"${ADB[@]}" logcat -d -s "VEXA:*" "Vexa-*:*" "DEBUG:*" "AndroidRuntime:E" "libc:*" \
  > "${OUT_DIR}/logcat_vexa_tags.txt" 2>/dev/null || true
"${ADB[@]}" logcat -d -b crash > "${OUT_DIR}/logcat_crash.txt" 2>/dev/null || true
echo "[pull-root-logs] logcat captured"

# --- app files (via root, no run-as needed) ---------------------------------
{
  su_run "find /data/data/${PKG} -type f -printf '%s %p\n' 2>/dev/null | sort -n"
} > "${OUT_DIR}/app_files.txt"

REMOTE_ARTS="$(su_run "find /data/data/${PKG}/files/artifacts /data/data/${PKG}/files/root_logs -type f 2>/dev/null" | tr -d '\r')"
for rf in ${REMOTE_ARTS}; do
  rel="${rf#*/files/}"
  lf="${OUT_DIR}/app_files/${rel}"
  mkdir -p "$(dirname "${lf}")"
  su_pull "${rf}" "${lf}"
done

# --- watch mode -------------------------------------------------------------
if ((WATCH_SECS > 0)); then
  echo "[pull-root-logs] watch: streaming dmesg + logcat for ${WATCH_SECS}s ..."
  "${ADB[@]}" shell "su -c 'dmesg -w'" > "${OUT_DIR}/dmesg_stream.log" 2>/dev/null &
  DMESG_PID=$!
  "${ADB[@]}" logcat -b all > "${OUT_DIR}/logcat_stream.log" 2>/dev/null &
  LOGCAT_PID=$!

  deadline=$((SECONDS + WATCH_SECS))
  n=0
  while ((SECONDS < deadline)); do
    wpid="$(su_run "pidof -s '${PKG}:runtime_worker'" | tr -d '\r')"
    if [[ -n "${wpid}" && ${DEBUGGERD} -eq 1 ]]; then
      n=$((n + 1))
      su_run "debuggerd -b ${wpid}" > "${OUT_DIR}/worker_backtrace_${n}_pid${wpid}.txt"
    fi
    sleep 3
  done
  kill "${DMESG_PID}" "${LOGCAT_PID}" 2>/dev/null || true
  echo "[pull-root-logs] watch done: ${n} worker backtraces"
fi

echo "[pull-root-logs] done -> ${OUT_DIR}"
