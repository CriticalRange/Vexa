#!/system/bin/sh
# vexa_proc_dump.sh <pid> <out_dir> [jit]
# Runs ON DEVICE (typically under `su`). Dumps per-process diagnostics into
# <out_dir> on /data/local/tmp so host scripts avoid su -c quoting hell.
# Optional 3rd arg "jit" also dumps rwxp (JIT/executable) memory regions from
# /proc/<pid>/mem into <out_dir>/jit/.

pid="$1"
out="$2"
mode="${3:-}"
mkdir -p "$out" 2>/dev/null

cat "/proc/$pid/status"        > "$out/status.txt"        2>/dev/null
cat "/proc/$pid/maps"          > "$out/maps.txt"          2>/dev/null
cat "/proc/$pid/smaps"         > "$out/smaps.txt"         2>/dev/null
cat "/proc/$pid/smaps_rollup"  > "$out/smaps_rollup.txt"  2>/dev/null
cat "/proc/$pid/io"            > "$out/io.txt"            2>/dev/null
cat "/proc/$pid/limits"        > "$out/limits.txt"        2>/dev/null
cat "/proc/$pid/mountinfo"     > "$out/mountinfo.txt"     2>/dev/null
cat "/proc/$pid/environ"       > "$out/environ.bin"       2>/dev/null
od -A x -t x8 "/proc/$pid/auxv" > "$out/auxv.txt"         2>/dev/null
for n in tcp tcp6 udp udp6 unix; do
  cat "/proc/$pid/net/$n" > "$out/net_$n.txt" 2>/dev/null
done

{
  echo "--- cmdline ---"
  tr '\0' ' ' < "/proc/$pid/cmdline"; echo
  echo "--- exe / cwd ---"
  readlink "/proc/$pid/exe" 2>/dev/null
  readlink "/proc/$pid/cwd" 2>/dev/null
  echo "--- selinux ctx ---"
  cat "/proc/$pid/attr/current" 2>/dev/null
  echo "--- cpuset ---"
  cat "/proc/$pid/cpuset" 2>/dev/null
  echo "--- cgroup ---"
  cat "/proc/$pid/cgroup" 2>/dev/null
  echo "--- affinity ---"
  taskset -p "$pid" 2>/dev/null
  echo "--- oom ---"
  echo "oom_score=$(cat "/proc/$pid/oom_score" 2>/dev/null) adj=$(cat "/proc/$pid/oom_score_adj" 2>/dev/null)"
  echo "--- live kernel state ---"
  echo "syscall: $(cat "/proc/$pid/syscall" 2>/dev/null)"
  echo "stack:"
  cat "/proc/$pid/stack" 2>/dev/null
  echo "--- sched ---"
  echo "schedstat: $(cat "/proc/$pid/schedstat" 2>/dev/null)"
  head -30 "/proc/$pid/sched" 2>/dev/null
  echo "--- timerslack_ns ---"
  cat "/proc/$pid/timerslack_ns" 2>/dev/null
  echo "--- personality / coredump_filter ---"
  cat "/proc/$pid/personality" "/proc/$pid/coredump_filter" 2>/dev/null
  echo "--- fd count ---"
  ls "/proc/$pid/fd" 2>/dev/null | wc -l
} > "$out/summary.txt" 2>/dev/null

# fd table with link targets (ashmem/dmabuf/sockets/pipes visible)
{
  for f in "/proc/$pid/fd"/*; do
    echo "$(basename "$f") -> $(readlink "$f" 2>/dev/null)"
  done
} > "$out/fds.txt" 2>/dev/null

# dmabuf/sync fdinfo: fence state for surface/graphical debugging
{
  for f in "/proc/$pid/fd"/*; do
    t="$(readlink "$f" 2>/dev/null)"
    case "$t" in
      *dmabuf*|*dma_heap*|*sync_file*|*sw_sync*)
        echo "=== fd $(basename "$f") $t ==="
        cat "/proc/$pid/fdinfo/$(basename "$f")" 2>/dev/null
        ;;
    esac
  done
} > "$out/fdinfo_dma.txt" 2>/dev/null

# per-task detail: state, wchan, cpu accounting, kernel stack
{
  for t in "/proc/$pid/task"/*; do
    tid="$(basename "$t")"
    comm="$(cat "$t/comm" 2>/dev/null)"
    wchan="$(cat "$t/wchan" 2>/dev/null)"
    stat_state="$(awk '/^State:/{print $2; exit}' "$t/status" 2>/dev/null)"
    cpu="$(sed 's/^.*) //' "$t/stat" 2>/dev/null | awk '{print "utime="$12" stime="$13" cpu="$37}')"
    echo "== tid $tid $comm state=$stat_state wchan=$wchan $cpu"
    echo "   kstack: $(tr '\n' ' ' < "$t/stack" 2>/dev/null | head -c 400)"
  done
} > "$out/tasks.txt" 2>/dev/null

command -v debuggerd >/dev/null 2>&1 && \
  debuggerd -b "$pid" > "$out/debuggerd_backtrace.txt" 2>&1

# optional: dump executable-anon (JIT/code-cache) regions from /proc/<pid>/mem
# for offline disasm. FEX uses W^X (r-x/rw- split) so rwxp alone is not enough.
if [ "$mode" = "jit" ]; then
  mkdir -p "$out/jit" 2>/dev/null
  awk '($2 ~ /x/ && ($6 == "" || $6 ~ /memfd|anon/)) || $2 == "rwxp" {print $1}' \
    "/proc/$pid/maps" 2>/dev/null | \
  while read -r range; do
    start="${range%-*}"; end="${range#*-}"
    s=$((16#$start)); e=$((16#$end)); sz=$((e - s))
    if [ "$sz" -gt 0 ] && [ "$sz" -le 67108864 ]; then
      dd if="/proc/$pid/mem" of="$out/jit/${start}-${end}.bin" \
         bs=4096 skip=$((s / 4096)) count=$((sz / 4096)) 2>/dev/null
    fi
  done
  echo "jit regions dumped"
fi

echo "done $pid"
