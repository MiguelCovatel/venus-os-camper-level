#!/bin/sh
set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP_DIR=/data/apps/dbus-camper-level
CONF_DIR=/data/conf/camper-level
SERVICE_LINK=/service/dbus-camper-level

if [ "$(id -u)" -ne 0 ]; then
    echo "Run this installer as root on Venus OS" >&2
    exit 1
fi

mkdir -p "$APP_DIR/service/log" "$CONF_DIR"
for file in main.py config.py mqtt_receiver.py dbus_service.py requirements.txt; do
    cp "$SOURCE_DIR/$file" "$APP_DIR/$file"
    chmod 0644 "$APP_DIR/$file"
done
cp "$SOURCE_DIR/service/run" "$APP_DIR/service/run"
cp "$SOURCE_DIR/service/log/run" "$APP_DIR/service/log/run"
chmod 0755 "$APP_DIR/service/run" "$APP_DIR/service/log/run"

if [ -d "$SOURCE_DIR/vendor" ]; then
    mkdir -p "$APP_DIR/vendor"
    cp -R "$SOURCE_DIR/vendor/." "$APP_DIR/vendor/"
elif [ -d "$SOURCE_DIR/wheels" ]; then
    mkdir -p "$APP_DIR/vendor"
    python3 -m pip install --no-index --target "$APP_DIR/vendor" "$SOURCE_DIR"/wheels/paho_mqtt*.whl
fi

if ! PYTHONPATH="$APP_DIR/vendor${PYTHONPATH:+:$PYTHONPATH}" python3 -c 'import paho.mqtt.client' >/dev/null 2>&1; then
    echo "paho-mqtt is missing. Use the release bundle with vendor/ or wheels/." >&2
    exit 2
fi

if [ ! -f "$CONF_DIR/config.ini" ]; then
    cp "$SOURCE_DIR/config.example.ini" "$CONF_DIR/config.ini"
    chmod 0600 "$CONF_DIR/config.ini"
    echo "Created $CONF_DIR/config.ini; configure the MQTT broker before use."
fi
if [ ! -f "$CONF_DIR/environment" ]; then
    cp "$SOURCE_DIR/environment.example" "$CONF_DIR/environment"
    chmod 0600 "$CONF_DIR/environment"
fi

if [ -e "$SERVICE_LINK" ] && [ ! -L "$SERVICE_LINK" ]; then
    echo "$SERVICE_LINK exists and is not a symlink; refusing to overwrite it" >&2
    exit 3
fi
ln -sfn "$APP_DIR/service" "$SERVICE_LINK"
if command -v svc >/dev/null 2>&1; then
    svc -t "$SERVICE_LINK" 2>/dev/null || svc -u "$SERVICE_LINK"
fi
echo "dbus-camper-level installed. Configuration was preserved in $CONF_DIR."
