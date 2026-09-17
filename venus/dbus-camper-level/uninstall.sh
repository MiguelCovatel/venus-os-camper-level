#!/bin/sh
set -eu

SERVICE_LINK=/service/dbus-camper-level
if [ "$(id -u)" -ne 0 ]; then
    echo "Run this uninstaller as root on Venus OS" >&2
    exit 1
fi
if [ -L "$SERVICE_LINK" ]; then
    command -v svc >/dev/null 2>&1 && svc -d "$SERVICE_LINK" 2>/dev/null || true
    rm -f "$SERVICE_LINK"
fi
echo "dbus-camper-level disabled. Application and /data/conf/camper-level were preserved."
