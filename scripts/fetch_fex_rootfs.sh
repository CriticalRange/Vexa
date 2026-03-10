#!/usr/bin/env bash
set -euo pipefail

# Fetches official FEX RootFS images and stores them in:
#   ${FEX_APP_DATA_LOCATION:-<repo>/.fex-emu}
#
# Source list:
#   https://raw.githubusercontent.com/FEX-Emu/RootFS/main/RootFS_links.json

LINKS_URL="${LINKS_URL:-https://raw.githubusercontent.com/FEX-Emu/RootFS/main/RootFS_links.json}"

# Defaults are practical for Android bring-up.
DISTRO="${DISTRO:-ubuntu}"
VERSION="${VERSION:-24.04}"
FSTYPE="${FSTYPE:-squashfs}" # squashfs | erofs
EXTRACT="${EXTRACT:-0}"       # 1 to unsquashfs into folder
SET_DEFAULT="${SET_DEFAULT:-0}" # 1 to update ~/.fex-emu/Config.json RootFS key

usage() {
  cat <<'EOF'
Usage: fetch_fex_rootfs.sh [options]

Options:
  --distro <name>       Distro match key (ubuntu, fedora, arch)
  --version <ver>       Distro version (eg: 24.04, 43, rolling)
  --type <kind>         squashfs or erofs (default: squashfs)
  --extract             Extract squashfs image after download
  --set-default         Set RootFS in ~/.fex-emu/Config.json
  --list                List available entries from upstream JSON
  --help                Show this help

Environment:
  LINKS_URL             Override links JSON URL
  FEX_APP_DATA_LOCATION Override base data directory
EOF
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing dependency: $1" >&2
    exit 1
  }
}

list_entries() {
  local json_path="$1"
  python3 - "$json_path" <<'PY'
import json,sys
data=json.load(open(sys.argv[1], "r", encoding="utf-8"))
for name, meta in data.get("v1", {}).items():
    print(f"{name}\t{meta.get('DistroMatch','')}\t{meta.get('DistroVersion','')}\t{meta.get('Type','')}\t{meta.get('URL','')}")
PY
}

select_entry() {
  local json_path="$1"
  python3 - "$json_path" "$DISTRO" "$VERSION" "$FSTYPE" <<'PY'
import json,sys
path,distro,version,fstype = sys.argv[1:]
data=json.load(open(path, "r", encoding="utf-8")).get("v1", {})

# 1) strict match on distro+version+type
for name, meta in data.items():
    if (meta.get("DistroMatch","").lower() == distro.lower() and
        str(meta.get("DistroVersion","")) == version and
        meta.get("Type","").lower() == fstype.lower()):
        print(name)
        print(meta.get("URL",""))
        sys.exit(0)

# 2) fallback distro+type
for name, meta in data.items():
    if (meta.get("DistroMatch","").lower() == distro.lower() and
        meta.get("Type","").lower() == fstype.lower()):
        print(name)
        print(meta.get("URL",""))
        sys.exit(0)

# 3) final fallback: Ubuntu 24.04 + requested type
for name, meta in data.items():
    if (meta.get("DistroMatch","").lower() == "ubuntu" and
        str(meta.get("DistroVersion","")) == "24.04" and
        meta.get("Type","").lower() == fstype.lower()):
        print(name)
        print(meta.get("URL",""))
        sys.exit(0)

sys.exit(1)
PY
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --distro) DISTRO="${2:?missing value}"; shift 2 ;;
    --version) VERSION="${2:?missing value}"; shift 2 ;;
    --type) FSTYPE="${2:?missing value}"; shift 2 ;;
    --extract) EXTRACT=1; shift ;;
    --set-default) SET_DEFAULT=1; shift ;;
    --list) LIST_ONLY=1; shift ;;
    --help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

require_cmd curl
require_cmd python3

if [[ "$FSTYPE" != "squashfs" && "$FSTYPE" != "erofs" ]]; then
  echo "--type must be squashfs or erofs" >&2
  exit 1
fi

if [[ "${LIST_ONLY:-0}" == "1" ]]; then
  tmp="$(mktemp)"
  trap 'rm -f "$tmp"' EXIT
  curl -fL --retry 3 --retry-all-errors "$LINKS_URL" -o "$tmp"
  list_entries "$tmp"
  exit 0
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_BASE_DATA="$SCRIPT_DIR/../.fex-emu"
BASE_DATA="${FEX_APP_DATA_LOCATION:-$DEFAULT_BASE_DATA}"
mkdir -p "$BASE_DATA"

tmp_json="$(mktemp)"
trap 'rm -f "$tmp_json"' EXIT
curl -fL --retry 3 --retry-all-errors "$LINKS_URL" -o "$tmp_json"

selection="$(select_entry "$tmp_json" || true)"
if [[ -z "$selection" ]]; then
  echo "No matching rootfs entry found for distro=$DISTRO version=$VERSION type=$FSTYPE" >&2
  echo "Use --list to inspect available entries." >&2
  exit 1
fi

ROOTFS_NAME="$(printf '%s\n' "$selection" | sed -n '1p')"
ROOTFS_URL="$(printf '%s\n' "$selection" | sed -n '2p')"
FILENAME="$(basename "$ROOTFS_URL")"
DEST="$BASE_DATA/$FILENAME"

echo "Selected: $ROOTFS_NAME"
echo "URL: $ROOTFS_URL"
echo "Destination: $DEST"

curl -fL --retry 3 --retry-all-errors --continue-at - "$ROOTFS_URL" -o "$DEST"
echo "Downloaded: $DEST"

if [[ "$EXTRACT" == "1" ]]; then
  if [[ "$FSTYPE" != "squashfs" ]]; then
    echo "--extract is currently supported only for squashfs entries." >&2
    exit 1
  fi
  require_cmd unsquashfs
  EXTRACT_DIR="$BASE_DATA/RootFS"
  mkdir -p "$EXTRACT_DIR"
  unsquashfs -f -d "$EXTRACT_DIR" "$DEST"
  echo "Extracted to: $EXTRACT_DIR"
fi

if [[ "$SET_DEFAULT" == "1" ]]; then
  CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/.fex-emu"
  CONFIG_PATH="$CONFIG_DIR/Config.json"
  mkdir -p "$CONFIG_DIR"
  python3 - "$CONFIG_PATH" "$FILENAME" <<'PY'
import json, os, sys
config_path = sys.argv[1]
rootfs_name = sys.argv[2]

data = {"Config": {}}
if os.path.exists(config_path):
    with open(config_path, "r", encoding="utf-8") as f:
        try:
            data = json.load(f)
        except json.JSONDecodeError:
            data = {"Config": {}}

if "Config" not in data or not isinstance(data["Config"], dict):
    data["Config"] = {}

data["Config"]["RootFS"] = rootfs_name

with open(config_path, "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2)
    f.write("\n")

print(f"Updated {config_path} RootFS={rootfs_name}")
PY
fi

echo "Done."
