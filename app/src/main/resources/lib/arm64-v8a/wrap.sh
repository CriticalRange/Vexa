#!/system/bin/sh
HERE="$(cd "$(dirname "$0")" && pwd)"
export LD_PRELOAD="${HERE}/libvexareserve.so${LD_PRELOAD:+:$LD_PRELOAD}"
exec "$@"
