#!/usr/bin/env bash
set -euo pipefail

# Push only the verified rootfs requirement set into app-internal storage.
#
# Default source:
#   <repo>/.fex-emu/RootFS
#
# Default destination (inside app sandbox):
#   files/rootfs
#
# Usage:
#   ./scripts/push_fex_rootfs.sh
#   ./scripts/push_fex_rootfs.sh --source /path/to/RootFS --package com.critical.vexaemulator
#   ./scripts/push_fex_rootfs.sh --clean --strict

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

PKG="com.critical.vexaemulator"
SOURCE_ROOTFS="${REPO_ROOT}/.fex-emu/RootFS"
DEST_ROOTFS_DIR="files/rootfs"
REMOTE_STAGE_ROOT="/data/local/tmp/vexa-rootfs-stage"
CLEAN_DEST=0
STRICT=0
INCLUDE_OPTIONAL=1

REQUIRED_ENTRIES=(
  "/lib64/ld-linux-x86-64.so.2"
  "/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2"
  "/lib/x86_64-linux-gnu/libc.so.6"
  "/lib/x86_64-linux-gnu/libm.so.6"
  "/lib/x86_64-linux-gnu/libdl.so.2"
  "/lib/x86_64-linux-gnu/libpthread.so.0"
  "/lib/x86_64-linux-gnu/libgcc_s.so.1"
  "/usr/lib/x86_64-linux-gnu/libstdc++.so.6"
  "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.33"
  "/usr/lib/x86_64-linux-gnu/libmvec.so.1"
  "/etc/ld.so.cache"
  "/etc/ld.so.conf"
  "/etc/ld.so.conf.d"
  "/usr/lib/x86_64-linux-gnu/libssl.so.3"
  "/usr/lib/x86_64-linux-gnu/libssl.so"
  "/usr/lib/x86_64-linux-gnu/libcrypto.so.3"
  "/usr/lib/x86_64-linux-gnu/libcrypto.so"
  "/usr/lib/i386-linux-gnu/libssl.so.3"
  "/usr/lib/i386-linux-gnu/libssl.so"
  "/usr/lib/i386-linux-gnu/libcrypto.so.3"
  "/usr/lib/i386-linux-gnu/libcrypto.so"
  "/usr/lib/x86_64-linux-gnu/libnss_dns.so.2"
  "/usr/lib/x86_64-linux-gnu/libnss_files.so.2"
  "/lib/x86_64-linux-gnu/libresolv.so.2"
  "/etc/nsswitch.conf"
  "/etc/resolv.conf"
  "/etc/hosts"
  "/etc/ssl/certs/ca-certificates.crt"
)

OPTIONAL_ENTRIES=(
  # Transitive deps required by libXdmcp/libxcb in current RootFS snapshots.
  "/usr/lib/x86_64-linux-gnu/libbsd.so.0"
  "/usr/lib/x86_64-linux-gnu/libmd.so.0"
)

usage() {
  cat <<'USAGE'
Usage: push_fex_rootfs.sh [options]

Options:
  --source <dir>      Source RootFS directory on host (default: <repo>/.fex-emu/RootFS)
  --package <name>    Android package name (default: com.critical.vexaemulator)
  --dest <path>       Destination under run-as root (default: files/rootfs)
  --clean             Clean destination first (default: disabled; overlay copy)
  --strict            Fail when any required source entry is missing
  --no-optional       Skip optional GL bridge entries
  --help              Show this help
USAGE
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "[push-fex-rootfs] missing command: $1" >&2
    exit 1
  }
}

ensure_file() {
  local path="$1"
  local content="$2"
  if [[ ! -f "$path" ]]; then
    mkdir -p "$(dirname "$path")"
    printf "%s\n" "$content" >"$path"
    echo "[push-fex-rootfs] created missing file: $path"
  fi
}

ensure_symlink() {
  local dir="$1"
  local link_name="$2"
  local target_name="$3"
  local link_path="$dir/$link_name"
  local target_path="$dir/$target_name"

  [[ -e "$target_path" ]] || return 0
  if [[ -L "$link_path" || -f "$link_path" ]]; then
    return 0
  fi

  (cd "$dir" && ln -s "$target_name" "$link_name")
  echo "[push-fex-rootfs] created symlink: $link_path -> $target_name"
}

ensure_nsswitch_hosts_dns() {
  local path="$1"
  if grep -Eq '^[[:space:]]*hosts:[[:space:]].*files.*dns' "$path"; then
    return 0
  fi
  if grep -Eq '^[[:space:]]*hosts:' "$path"; then
    sed -i -E 's|^[[:space:]]*hosts:.*$|hosts: files dns|' "$path"
  else
    printf "\nhosts: files dns\n" >>"$path"
  fi
  echo "[push-fex-rootfs] updated hosts resolver order in: $path"
}

report_path() {
  local rootfs="$1"
  local rel="$2"
  if [[ -e "$rootfs$rel" || -L "$rootfs$rel" ]]; then
    echo "[ok] $rel"
    return 0
  fi
  echo "[missing] $rel"
  return 1
}

validate_required_source() {
  local rootfs="$1"
  local missing=0
  local rel

  echo "[push-fex-rootfs] validating required rootfs files..."
  for rel in "${REQUIRED_ENTRIES[@]}"; do
    if ! report_path "$rootfs" "$rel"; then
      missing=1
    fi
  done

  echo "[push-fex-rootfs] checking optional GL proc bridge prerequisites..."
  for rel in "${OPTIONAL_ENTRIES[@]}"; do
    report_path "$rootfs" "$rel" || true
  done

  if [[ "$missing" -eq 1 && "$STRICT" -eq 1 ]]; then
    echo "[push-fex-rootfs] required entries missing in source rootfs (strict mode)." >&2
    return 1
  fi
  return 0
}

stage_path() {
  local rel="$1"
  local required="$2"
  local src="$SOURCE_ROOTFS$rel"

  if [[ ! -e "$src" && ! -L "$src" ]]; then
    if [[ "$required" -eq 1 ]]; then
      echo "[push-fex-rootfs] missing required source path: $src" >&2
      return 1
    fi
    return 0
  fi

  if [[ -f "$src" || -L "$src" ]]; then
    mkdir -p "$LOCAL_STAGE_DIR$(dirname "$rel")"
    cp -aL -- "$src" "$LOCAL_STAGE_DIR$rel"
    return 0
  fi

  if [[ -d "$src" ]]; then
    mkdir -p "$LOCAL_STAGE_DIR$rel"

    local item item_rel
    while IFS= read -r -d '' item; do
      item_rel="/${item#$SOURCE_ROOTFS/}"

      if [[ -d "$item" ]]; then
        mkdir -p "$LOCAL_STAGE_DIR$item_rel"
      elif [[ -f "$item" || -L "$item" ]]; then
        mkdir -p "$LOCAL_STAGE_DIR$(dirname "$item_rel")"
        cp -aL -- "$item" "$LOCAL_STAGE_DIR$item_rel"
      fi
    done < <(find "$src" -mindepth 1 -print0)

    return 0
  fi

  if [[ "$required" -eq 1 ]]; then
    echo "[push-fex-rootfs] unsupported required source type: $src" >&2
    return 1
  fi
  return 0
}

verify_device_required() {
  local missing=0
  local rel

  for rel in "${REQUIRED_ENTRIES[@]}"; do
    if ! adb shell run-as "$PKG" /system/bin/test -e "${DEST_ROOTFS_DIR}${rel}"; then
      echo "[push-fex-rootfs] missing on device: ${DEST_ROOTFS_DIR}${rel}" >&2
      missing=1
    fi
  done

  if [[ "$missing" -ne 0 ]]; then
    return 1
  fi
  return 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --source)
      SOURCE_ROOTFS="${2:?missing value for --source}"
      shift 2
      ;;
    --package)
      PKG="${2:?missing value for --package}"
      shift 2
      ;;
    --dest)
      DEST_ROOTFS_DIR="${2:?missing value for --dest}"
      shift 2
      ;;
    --clean)
      CLEAN_DEST=1
      shift
      ;;
    --strict)
      STRICT=1
      shift
      ;;
    --no-optional)
      INCLUDE_OPTIONAL=0
      shift
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "[push-fex-rootfs] unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

need_cmd adb
need_cmd grep
need_cmd sed
need_cmd find
need_cmd cp
need_cmd mktemp

if [[ ! -d "$SOURCE_ROOTFS" ]]; then
  echo "[push-fex-rootfs] source directory missing: $SOURCE_ROOTFS" >&2
  exit 1
fi

if [[ ! -d "$SOURCE_ROOTFS/usr" ]]; then
  echo "[push-fex-rootfs] source rootfs appears invalid (missing $SOURCE_ROOTFS/usr)" >&2
  exit 1
fi

echo "[push-fex-rootfs] preparing required rootfs config and symlinks..."
ensure_file "$SOURCE_ROOTFS/etc/resolv.conf" $'nameserver 8.8.8.8\nnameserver 8.8.4.4'
ensure_file "$SOURCE_ROOTFS/etc/hosts" $'127.0.0.1 localhost\n::1 localhost ip6-localhost ip6-loopback'
ensure_file "$SOURCE_ROOTFS/etc/nsswitch.conf" "hosts: files dns"
ensure_nsswitch_hosts_dns "$SOURCE_ROOTFS/etc/nsswitch.conf"
ensure_symlink "$SOURCE_ROOTFS/usr/lib/x86_64-linux-gnu" "libssl.so" "libssl.so.3"
ensure_symlink "$SOURCE_ROOTFS/usr/lib/x86_64-linux-gnu" "libcrypto.so" "libcrypto.so.3"
ensure_symlink "$SOURCE_ROOTFS/usr/lib/i386-linux-gnu" "libssl.so" "libssl.so.3"
ensure_symlink "$SOURCE_ROOTFS/usr/lib/i386-linux-gnu" "libcrypto.so" "libcrypto.so.3"

validate_required_source "$SOURCE_ROOTFS"

adb get-state >/dev/null 2>&1 || {
  echo "[push-fex-rootfs] no adb device connected" >&2
  exit 1
}

adb shell run-as "$PKG" /system/bin/true >/dev/null 2>&1 || {
  echo "[push-fex-rootfs] run-as failed for $PKG (debuggable app required)" >&2
  exit 1
}

echo "[push-fex-rootfs] package=$PKG"
echo "[push-fex-rootfs] source=$SOURCE_ROOTFS"
echo "[push-fex-rootfs] dest=$DEST_ROOTFS_DIR"

LOCAL_STAGE_DIR="$(mktemp -d)"

cleanup() {
  rm -rf "$LOCAL_STAGE_DIR" >/dev/null 2>&1 || true
  adb shell /system/bin/rm -rf "$REMOTE_STAGE_ROOT" >/dev/null 2>&1 || true
}
trap cleanup EXIT

for rel in "${REQUIRED_ENTRIES[@]}"; do
  stage_path "$rel" 1
done

if [[ "$INCLUDE_OPTIONAL" -eq 1 ]]; then
  for rel in "${OPTIONAL_ENTRIES[@]}"; do
    stage_path "$rel" 0
  done
fi

if [[ "$CLEAN_DEST" -eq 1 ]]; then
  adb shell run-as "$PKG" /system/bin/rm -rf "$DEST_ROOTFS_DIR"
fi
adb shell run-as "$PKG" /system/bin/mkdir -p "$DEST_ROOTFS_DIR"

adb shell /system/bin/rm -rf "$REMOTE_STAGE_ROOT" >/dev/null 2>&1 || true
adb shell /system/bin/mkdir -p "$REMOTE_STAGE_ROOT"

echo "[push-fex-rootfs] pushing verified rootfs set to temp..."
adb push -a "$LOCAL_STAGE_DIR/." "$REMOTE_STAGE_ROOT/" >/dev/null

echo "[push-fex-rootfs] copying verified rootfs set into $DEST_ROOTFS_DIR..."
adb shell run-as "$PKG" /system/bin/cp -R "$REMOTE_STAGE_ROOT/." "$DEST_ROOTFS_DIR"

verify_device_required || {
  echo "[push-fex-rootfs] verification failed" >&2
  exit 1
}

echo "[push-fex-rootfs] verification ok"
