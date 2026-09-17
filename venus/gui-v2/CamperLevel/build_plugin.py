#!/usr/bin/env python3
"""Build the CamperLevel GUI v2 plugin JSON with Qt rcc."""

from __future__ import annotations

import base64
import json
import os
import shutil
import subprocess
from pathlib import Path

PLUGIN_NAME = "CamperLevel"
PLUGIN_VERSION = "1.0.0"
DEVICE_PRODUCT_ID = "0xC512"
RESOURCE_PREFIX = f"{PLUGIN_NAME}_v{PLUGIN_VERSION.replace('.', '_')}"
QML_FILES = ("CamperLevel_Page.qml", "CamperLevel.qml", "WheelLiftIndicator.qml")


def find_rcc(source_dir: Path) -> str:
    configured = os.environ.get("RCC")
    if configured and Path(configured).is_file():
        return configured
    executable = shutil.which("rcc") or shutil.which("rcc.exe")
    if executable:
        return executable
    for parent in source_dir.parents:
        candidate = parent / ".tools" / "qt683" / "PySide6" / "rcc.exe"
        if candidate.is_file():
            return str(candidate)
    raise SystemExit("Qt rcc not found; set RCC or install PySide6-Essentials")


def main() -> int:
    source_dir = Path(__file__).resolve().parent
    build_dir = source_dir / "build"
    build_dir.mkdir(parents=True, exist_ok=True)
    qrc_path = build_dir / f"{PLUGIN_NAME}.qrc"
    rcc_path = build_dir / f"{PLUGIN_NAME}.rcc"
    json_path = build_dir / f"{PLUGIN_NAME}.json"
    entries = "\n".join(
        f'    <file alias="{filename}">{(source_dir / filename).as_posix()}</file>'
        for filename in QML_FILES
    )
    qrc_path.write_text(
        f'<RCC>\n  <qresource prefix="/{RESOURCE_PREFIX}">\n{entries}\n  </qresource>\n</RCC>\n',
        encoding="utf-8",
    )
    subprocess.run([find_rcc(source_dir), "-binary", "-compress-algo", "zlib",
                    "-o", str(rcc_path), str(qrc_path)], check=True)
    metadata = {
        "name": PLUGIN_NAME,
        "version": PLUGIN_VERSION,
        "minRequiredVersion": "v1.3.11",
        "maxRequiredVersion": "",
        "translations": [],
        "integrations": [
            {"type": 1, "url": f"qrc:/{RESOURCE_PREFIX}/CamperLevel_Page.qml"},
            {"type": 2, "productId": DEVICE_PRODUCT_ID, "title": "Camper Level",
             "url": f"qrc:/{RESOURCE_PREFIX}/CamperLevel_Page.qml"},
        ],
        "resource": base64.b64encode(rcc_path.read_bytes()).decode("ascii"),
    }
    json_path.write_text(json.dumps(metadata, indent=4) + "\n", encoding="utf-8")
    print(json_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
