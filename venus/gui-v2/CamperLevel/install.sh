#!/bin/sh
set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PLUGIN_JSON=${1:-$SOURCE_DIR/build/CamperLevel.json}
AVAILABLE_DIR=/data/apps/available/CamperLevel
ENABLED_LINK=/data/apps/enabled/CamperLevel

if [ "$(id -u)" -ne 0 ]; then
    echo "Run this installer as root on Venus OS" >&2
    exit 1
fi
if [ ! -f "$PLUGIN_JSON" ]; then
    echo "Plugin file not found: $PLUGIN_JSON" >&2
    exit 2
fi
mkdir -p "$AVAILABLE_DIR/gui-v2" /data/apps/enabled
cp "$PLUGIN_JSON" "$AVAILABLE_DIR/gui-v2/CamperLevel.json"
chmod 0644 "$AVAILABLE_DIR/gui-v2/CamperLevel.json"
if [ -e "$ENABLED_LINK" ] && [ ! -L "$ENABLED_LINK" ]; then
    echo "$ENABLED_LINK exists and is not a symlink; refusing to overwrite it" >&2
    exit 3
fi
ln -sfn "$AVAILABLE_DIR" "$ENABLED_LINK"
echo "CamperLevel GUI v2 plugin enabled. Restart GUI v2 or reload Remote Console."
