#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PURGE=false
if [ "${1:-}" = "--purge" ]; then PURGE=true; fi

"$ROOT/venus/gui-v2/CamperLevel/uninstall.sh"
"$ROOT/venus/dbus-camper-level/uninstall.sh"

if [ "$PURGE" = false ]; then
    echo "Camper Level disabled. Configuration was preserved."
else
    for target in \
        /data/apps/dbus-camper-level \
        /data/conf/camper-level \
        /data/apps/available/CamperLevel
    do
        case "$target" in
            /data/apps/dbus-camper-level|/data/conf/camper-level|/data/apps/available/CamperLevel)
                rm -rf -- "$target"
                ;;
            *) echo "Refusing unsafe purge target: $target" >&2; exit 3 ;;
        esac
    done
    echo "Camper Level purged from its dedicated application/configuration paths."
fi
