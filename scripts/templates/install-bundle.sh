#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CONFIG=/data/conf/camper-level/config.ini
ENVIRONMENT=/data/conf/camper-level/environment
MQTT_HOST=
MQTT_PORT=1883
MQTT_USER=
MQTT_PASSWORD=${CAMPER_LEVEL_MQTT_PASSWORD:-}
BASE_TOPIC=camper/level

usage() {
    echo "Usage: $0 [--mqtt-host HOST] [--mqtt-port PORT] [--mqtt-user USER] [--base-topic TOPIC]"
    echo "Set CAMPER_LEVEL_MQTT_PASSWORD instead of placing a password in shell history."
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --mqtt-host) MQTT_HOST=${2:?missing MQTT host}; shift 2 ;;
        --mqtt-port) MQTT_PORT=${2:?missing MQTT port}; shift 2 ;;
        --mqtt-user) MQTT_USER=${2:?missing MQTT user}; shift 2 ;;
        --base-topic) BASE_TOPIC=${2:?missing base topic}; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [ "$(id -u)" -ne 0 ]; then
    echo "Run this installer as root on Venus OS" >&2
    exit 1
fi

if [ -z "$MQTT_HOST" ] && [ -t 0 ]; then
    printf "MQTT broker address: "
    IFS= read -r MQTT_HOST
fi
if [ -z "$MQTT_HOST" ]; then
    echo "MQTT host is required (--mqtt-host HOST)" >&2
    exit 2
fi

if [ -z "$MQTT_PASSWORD" ] && [ -n "$MQTT_USER" ] && [ -t 0 ]; then
    printf "MQTT password (input hidden): "
    stty -echo
    IFS= read -r MQTT_PASSWORD
    stty echo
    printf "\n"
fi

"$ROOT/venus/dbus-camper-level/install.sh"
"$ROOT/venus/gui-v2/CamperLevel/install.sh" \
    "$ROOT/venus/gui-v2/CamperLevel/build/CamperLevel.json"

python3 - "$CONFIG" "$MQTT_HOST" "$MQTT_PORT" "$MQTT_USER" "$BASE_TOPIC" <<'PY'
import configparser
import os
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
parser = configparser.ConfigParser(interpolation=None)
parser.read(path, encoding="utf-8")
if not parser.has_section("mqtt"):
    parser.add_section("mqtt")
parser.set("mqtt", "host", sys.argv[2])
parser.set("mqtt", "port", sys.argv[3])
parser.set("mqtt", "username", sys.argv[4])
parser.set("mqtt", "password", "")
parser.set("mqtt", "password_env", "CAMPER_LEVEL_MQTT_PASSWORD")
parser.set("mqtt", "base_topic", sys.argv[5])
temporary = path.with_name(path.name + ".tmp")
with temporary.open("w", encoding="utf-8") as handle:
    parser.write(handle)
os.chmod(temporary, 0o600)
os.replace(temporary, path)
PY

if [ -n "$MQTT_PASSWORD" ]; then
    python3 - "$ENVIRONMENT" "$MQTT_PASSWORD" <<'PY'
import os
import pathlib
import shlex
import sys

path = pathlib.Path(sys.argv[1])
prefix = "CAMPER_LEVEL_MQTT_PASSWORD="
lines = path.read_text(encoding="utf-8").splitlines() if path.exists() else []
replacement = prefix + shlex.quote(sys.argv[2])
output = []
replaced = False
for line in lines:
    if line.startswith(prefix):
        if not replaced:
            output.append(replacement)
            replaced = True
    else:
        output.append(line)
if not replaced:
    output.append(replacement)
temporary = path.with_name(path.name + ".tmp")
temporary.write_text("\n".join(output) + "\n", encoding="utf-8")
os.chmod(temporary, 0o600)
os.replace(temporary, path)
PY
    chmod 0600 "$ENVIRONMENT"
fi
chmod 0600 "$CONFIG"

python3 /data/apps/dbus-camper-level/main.py --config "$CONFIG" --check-config
svc -t /service/dbus-camper-level 2>/dev/null || true

echo "Camper Level installed. Configuration was preserved or updated in $CONFIG"
echo "Open Settings -> Integrations -> CamperLevel after the service reconnects."
