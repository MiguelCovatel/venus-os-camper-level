#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CONFIG=/data/conf/camper-level/config.ini
ENVIRONMENT=/data/conf/camper-level/environment
MQTT_HOST=
MQTT_HOST_SET=false
MQTT_PORT=
MQTT_PORT_SET=false
MQTT_USER=
MQTT_USER_SET=false
MQTT_PASSWORD=${CAMPER_LEVEL_MQTT_PASSWORD:-}
PASSWORD_ACTION=preserve
BASE_TOPIC=
BASE_TOPIC_SET=false
USE_VENUS_MQTT=false

usage() {
    echo "Usage: $0 [--use-venus-mqtt] [--mqtt-host HOST] [--mqtt-port PORT]"
    echo "          [--mqtt-user USER] [--base-topic TOPIC]"
    echo
    echo "With no MQTT options, a new installation uses the broker built into Venus OS."
    echo "Use --use-venus-mqtt to switch an existing installation back to that mode."
    echo "The --mqtt-* options are only for an external/advanced broker."
    echo "Set CAMPER_LEVEL_MQTT_PASSWORD instead of placing a password in shell history."
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --use-venus-mqtt) USE_VENUS_MQTT=true; shift ;;
        --mqtt-host) MQTT_HOST=${2:?missing MQTT host}; MQTT_HOST_SET=true; shift 2 ;;
        --mqtt-port) MQTT_PORT=${2:?missing MQTT port}; MQTT_PORT_SET=true; shift 2 ;;
        --mqtt-user) MQTT_USER=${2:?missing MQTT user}; MQTT_USER_SET=true; shift 2 ;;
        --base-topic) BASE_TOPIC=${2:?missing base topic}; BASE_TOPIC_SET=true; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [ "$(id -u)" -ne 0 ]; then
    echo "Run this installer as root on Venus OS" >&2
    exit 1
fi

if [ "$USE_VENUS_MQTT" = true ] && [ "$MQTT_HOST_SET" = true ]; then
    echo "Choose either --use-venus-mqtt or --mqtt-host, not both." >&2
    exit 2
fi

CONFIG_WAS_PRESENT=false
if [ -f "$CONFIG" ]; then CONFIG_WAS_PRESENT=true; fi

if [ "$USE_VENUS_MQTT" = true ] || \
   { [ "$CONFIG_WAS_PRESENT" = false ] && [ "$MQTT_HOST_SET" = false ]; }; then
    MQTT_HOST=127.0.0.1
    MQTT_HOST_SET=true
    MQTT_PORT=1883
    MQTT_PORT_SET=true
    MQTT_USER=
    MQTT_USER_SET=true
    MQTT_PASSWORD=
    PASSWORD_ACTION=clear
fi

if [ -n "$MQTT_PASSWORD" ] && [ "$PASSWORD_ACTION" != clear ]; then
    PASSWORD_ACTION=set
fi
if [ -z "$MQTT_PASSWORD" ] && [ "$MQTT_USER_SET" = true ] && [ -n "$MQTT_USER" ] && [ -t 0 ]; then
    printf "MQTT password (input hidden): "
    stty -echo
    IFS= read -r MQTT_PASSWORD
    stty echo
    printf "\n"
    if [ -n "$MQTT_PASSWORD" ]; then PASSWORD_ACTION=set; fi
fi

"$ROOT/venus/dbus-camper-level/install.sh"
"$ROOT/venus/gui-v2/CamperLevel/install.sh" \
    "$ROOT/venus/gui-v2/CamperLevel/build/CamperLevel.json"

python3 - "$CONFIG" \
    "$MQTT_HOST_SET" "$MQTT_HOST" \
    "$MQTT_PORT_SET" "$MQTT_PORT" \
    "$MQTT_USER_SET" "$MQTT_USER" \
    "$BASE_TOPIC_SET" "$BASE_TOPIC" <<'PY'
import configparser
import os
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
parser = configparser.ConfigParser(interpolation=None)
parser.read(path, encoding="utf-8")
if not parser.has_section("mqtt"):
    parser.add_section("mqtt")
if sys.argv[2] == "true":
    parser.set("mqtt", "host", sys.argv[3])
if sys.argv[4] == "true":
    parser.set("mqtt", "port", sys.argv[5])
if sys.argv[6] == "true":
    parser.set("mqtt", "username", sys.argv[7])
parser.set("mqtt", "password", "")
parser.set("mqtt", "password_env", "CAMPER_LEVEL_MQTT_PASSWORD")
if sys.argv[8] == "true":
    parser.set("mqtt", "base_topic", sys.argv[9])
temporary = path.with_name(path.name + ".tmp")
with temporary.open("w", encoding="utf-8") as handle:
    parser.write(handle)
os.chmod(temporary, 0o600)
os.replace(temporary, path)
PY

python3 - "$ENVIRONMENT" "$PASSWORD_ACTION" "$MQTT_PASSWORD" <<'PY'
import os
import pathlib
import shlex
import sys

path = pathlib.Path(sys.argv[1])
action = sys.argv[2]
prefix = "CAMPER_LEVEL_MQTT_PASSWORD="
lines = path.read_text(encoding="utf-8").splitlines() if path.exists() else []
replacement = prefix + shlex.quote(sys.argv[3])
output = []
replaced = False
for line in lines:
    if line.startswith(prefix):
        if action == "set" and not replaced:
            output.append(replacement)
            replaced = True
        elif action == "preserve":
            output.append(line)
    else:
        output.append(line)
if action == "set" and not replaced:
    output.append(replacement)
temporary = path.with_name(path.name + ".tmp")
temporary.write_text("\n".join(output) + "\n", encoding="utf-8")
os.chmod(temporary, 0o600)
os.replace(temporary, path)
PY
chmod 0600 "$ENVIRONMENT"
chmod 0600 "$CONFIG"

MQTT_EFFECTIVE_HOST=$(python3 - "$CONFIG" <<'PY'
import configparser, sys
p = configparser.ConfigParser(interpolation=None)
p.read(sys.argv[1], encoding="utf-8")
print(p.get("mqtt", "host", fallback=""))
PY
)
MQTT_EFFECTIVE_PORT=$(python3 - "$CONFIG" <<'PY'
import configparser, sys
p = configparser.ConfigParser(interpolation=None)
p.read(sys.argv[1], encoding="utf-8")
print(p.get("mqtt", "port", fallback="1883"))
PY
)
MQTT_EFFECTIVE_TOPIC=$(python3 - "$CONFIG" <<'PY'
import configparser, sys
p = configparser.ConfigParser(interpolation=None)
p.read(sys.argv[1], encoding="utf-8")
print(p.get("mqtt", "base_topic", fallback="camper/level"))
PY
)

LOCAL_BROKER=false
case "$MQTT_EFFECTIVE_HOST" in
    127.0.0.1|localhost|::1) LOCAL_BROKER=true ;;
esac

if ! python3 - "$MQTT_EFFECTIVE_HOST" "$MQTT_EFFECTIVE_PORT" <<'PY'
import socket
import sys
import time

host, port = sys.argv[1], int(sys.argv[2])
deadline = time.monotonic() + 8
while True:
    try:
        with socket.create_connection((host, port), timeout=2):
            raise SystemExit(0)
    except OSError:
        if time.monotonic() >= deadline:
            raise SystemExit(1)
        time.sleep(1)
PY
then
    if [ "$LOCAL_BROKER" = true ]; then
        echo "Venus OS MQTT Access is not enabled." >&2
        echo "Enable Settings -> Integrations -> MQTT Access, then run this installer again." >&2
    else
        echo "The external MQTT broker $MQTT_EFFECTIVE_HOST:$MQTT_EFFECTIVE_PORT is unreachable from Venus OS." >&2
    fi
    exit 4
fi

python3 /data/apps/dbus-camper-level/main.py --config "$CONFIG" --check-config
svc -t /service/dbus-camper-level 2>/dev/null || true

echo "Camper Level installed. Configuration was preserved or updated in $CONFIG"
if [ "$LOCAL_BROKER" = true ]; then
    echo
    echo "VENUS OS MQTT IS READY"
    echo "On the ESP32 setup page enter:"
    echo "  Server: the local IP address of this Venus OS device"
    echo "  Port:   1883"
    echo "  User:   leave empty"
    echo "  Password: leave empty"
    echo "  Topic:  $MQTT_EFFECTIVE_TOPIC"
    echo "Find the IP under Settings -> Connectivity -> Wi-Fi/Ethernet -> IP address."
    echo "Use MQTT only on the trusted camper LAN and never port-forward port 1883."
else
    echo "External MQTT broker: $MQTT_EFFECTIVE_HOST:$MQTT_EFFECTIVE_PORT"
fi
echo "Open Settings -> Integrations -> CamperLevel after the service reconnects."
