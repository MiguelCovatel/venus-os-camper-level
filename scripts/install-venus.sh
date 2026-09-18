#!/bin/sh
set -eu

REPOSITORY=${CAMPER_LEVEL_REPOSITORY:-MiguelCovatel/venus-os-camper-level}
VERSION=${CAMPER_LEVEL_VERSION:-latest}
ARCHIVE=venus-os-camper-level-venus.tar.gz
CHECKSUM=$ARCHIVE.sha256

case "$REPOSITORY" in
    *'<'*|*'>'*)
        echo "Set CAMPER_LEVEL_REPOSITORY=owner/venus-os-camper-level" >&2
        exit 2
        ;;
esac

if [ "$VERSION" = latest ]; then
    BASE_URL=
else
    BASE_URL="https://github.com/$REPOSITORY/releases/download/$VERSION"
fi

if command -v curl >/dev/null 2>&1; then
    fetch() { curl -fL --retry 3 --connect-timeout 20 -o "$2" "$1"; }
elif command -v wget >/dev/null 2>&1; then
    fetch() { wget -O "$2" "$1"; }
else
    echo "curl or wget is required" >&2
    exit 3
fi

TMP_ROOT=${TMPDIR:-/tmp}
WORK_DIR=$(mktemp -d "$TMP_ROOT/camper-level.XXXXXX")
cleanup() { rm -rf -- "$WORK_DIR"; }
trap cleanup EXIT HUP INT TERM

if [ "$VERSION" = latest ]; then
    RELEASES_JSON="$WORK_DIR/releases.json"
    fetch "https://api.github.com/repos/$REPOSITORY/releases?per_page=1" "$RELEASES_JSON"
    VERSION=$(python3 - "$RELEASES_JSON" <<'PY'
import json
import pathlib
import sys

releases = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
if not releases or not releases[0].get("tag_name"):
    raise SystemExit("No published GitHub release was found")
print(releases[0]["tag_name"])
PY
)
    BASE_URL="https://github.com/$REPOSITORY/releases/download/$VERSION"
fi

fetch "$BASE_URL/$ARCHIVE" "$WORK_DIR/$ARCHIVE"
fetch "$BASE_URL/$CHECKSUM" "$WORK_DIR/$CHECKSUM"

if ! command -v sha256sum >/dev/null 2>&1; then
    echo "sha256sum is required to verify the package" >&2
    exit 4
fi
(cd "$WORK_DIR" && sha256sum -c "$CHECKSUM")

mkdir "$WORK_DIR/unpacked"
tar -xzf "$WORK_DIR/$ARCHIVE" -C "$WORK_DIR/unpacked"
INSTALLER="$WORK_DIR/unpacked/venus-os-camper-level-venus/install.sh"
if [ ! -f "$INSTALLER" ]; then
    echo "The verified package does not contain the expected installer" >&2
    exit 5
fi

chmod 0755 "$INSTALLER"
"$INSTALLER" "$@"
