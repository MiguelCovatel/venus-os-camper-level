#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DIST="$ROOT/dist"
FIRMWARE_BUILD="$ROOT/firmware/.pio/build/esp32-c3-supermini"
FIRMWARE_STAGE="$DIST/camper-level-firmware"
VENUS_STAGE="$DIST/venus-os-camper-level-venus"

case "$DIST" in "$ROOT"/*) ;; *) echo "Unsafe dist path" >&2; exit 2 ;; esac
rm -rf -- "$DIST"
mkdir -p "$FIRMWARE_STAGE" "$VENUS_STAGE"

pio run -d "$ROOT/firmware" -e esp32-c3-supermini

for file in bootloader.bin partitions.bin firmware.bin; do
    test -f "$FIRMWARE_BUILD/$file" || { echo "Missing $file" >&2; exit 3; }
    cp "$FIRMWARE_BUILD/$file" "$FIRMWARE_STAGE/$file"
done

PIO_HOME=${PLATFORMIO_CORE_DIR:-${HOME:?}/.platformio}
BOOT_APP0=$(find "$PIO_HOME/packages/framework-arduinoespressif32" -type f \
    -path '*/tools/partitions/boot_app0.bin' -print -quit)
test -n "$BOOT_APP0" || { echo "boot_app0.bin not found" >&2; exit 4; }
cp "$BOOT_APP0" "$FIRMWARE_STAGE/boot_app0.bin"
ESPTOOL="$PIO_HOME/packages/tool-esptoolpy/esptool.py"
test -f "$ESPTOOL" || { echo "PlatformIO esptool.py not found" >&2; exit 4; }
python3 "$ESPTOOL" --chip esp32c3 merge_bin \
    -o "$FIRMWARE_STAGE/camper-level-factory.bin" \
    --flash_mode dio --flash_freq 80m --flash_size 4MB \
    0x0000 "$FIRMWARE_STAGE/bootloader.bin" \
    0x8000 "$FIRMWARE_STAGE/partitions.bin" \
    0xE000 "$FIRMWARE_STAGE/boot_app0.bin" \
    0x10000 "$FIRMWARE_STAGE/firmware.bin"
cp "$ROOT/scripts/flash-firmware.ps1" "$FIRMWARE_STAGE/flash-firmware.ps1"
cp "$ROOT/scripts/configure-esp.py" "$FIRMWARE_STAGE/configure-esp.py"
cp "$ROOT/docs/INICIO-RAPIDO.es.md" "$FIRMWARE_STAGE/LEEME.md"
cp "$ROOT/LICENSE" "$FIRMWARE_STAGE/LICENSE"

python3 - "$FIRMWARE_STAGE" "$DIST/camper-level-firmware.zip" <<'PY'
import pathlib
import sys
import zipfile

source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(source.iterdir()):
        archive.write(path, path.name)
PY

python3 "$ROOT/venus/gui-v2/CamperLevel/build_plugin.py"
mkdir -p "$VENUS_STAGE/venus"
cp -R "$ROOT/venus/dbus-camper-level" "$VENUS_STAGE/venus/"
cp -R "$ROOT/venus/gui-v2" "$VENUS_STAGE/venus/"
cp "$ROOT/scripts/templates/install-bundle.sh" "$VENUS_STAGE/install.sh"
cp "$ROOT/scripts/templates/uninstall-bundle.sh" "$VENUS_STAGE/uninstall.sh"
cp "$ROOT/README.md" "$VENUS_STAGE/README.md"
cp "$ROOT/LICENSE" "$VENUS_STAGE/LICENSE"
cp "$ROOT/THIRD_PARTY_NOTICES.md" "$VENUS_STAGE/THIRD_PARTY_NOTICES.md"
chmod 0755 "$VENUS_STAGE/install.sh" "$VENUS_STAGE/uninstall.sh"

DAEMON="$VENUS_STAGE/venus/dbus-camper-level"
rm -rf -- "$DAEMON/vendor"
python3 -m pip install --disable-pip-version-check --no-user --ignore-installed \
    --no-compile --no-cache-dir \
    --target "$DAEMON/vendor" -r "$DAEMON/requirements.txt"

if ! find "$DAEMON/vendor" -type f \( -iname 'LICENSE*' -o -iname 'COPYING*' \) \
    -print -quit | grep -q .; then
    echo "Vendored dependencies have no discoverable license file; refusing release" >&2
    exit 5
fi

find "$VENUS_STAGE" -type d -name __pycache__ -prune -exec rm -rf -- {} +
find "$VENUS_STAGE" -type f \( -name '*.pyc' -o -name '*.pyo' \) -delete

(cd "$DIST" && tar -czf venus-os-camper-level-venus.tar.gz venus-os-camper-level-venus)
(cd "$DIST" && sha256sum camper-level-firmware.zip > camper-level-firmware.zip.sha256)
(cd "$DIST" && sha256sum venus-os-camper-level-venus.tar.gz \
    > venus-os-camper-level-venus.tar.gz.sha256)

echo "Release artifacts created in $DIST"
