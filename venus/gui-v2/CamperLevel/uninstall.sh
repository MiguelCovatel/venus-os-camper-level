#!/bin/sh
set -eu

ENABLED_LINK=/data/apps/enabled/CamperLevel
if [ "$(id -u)" -ne 0 ]; then
    echo "Run this uninstaller as root on Venus OS" >&2
    exit 1
fi
if [ -L "$ENABLED_LINK" ]; then
    rm -f "$ENABLED_LINK"
fi
echo "CamperLevel GUI v2 plugin disabled; installed files and settings were preserved."
