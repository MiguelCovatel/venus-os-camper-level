#!/usr/bin/env python3
"""Validate a fully assembled Camper Level GitHub Pages directory."""

from __future__ import annotations

import json
import re
import sys
import xml.etree.ElementTree as ET
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit


class References(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.paths: set[str] = set()

    def handle_starttag(self, _tag: str, attrs: list[tuple[str, str | None]]) -> None:
        for name, value in attrs:
            if name not in {"src", "href"} or not value:
                continue
            parsed = urlsplit(value)
            if not parsed.scheme and not parsed.netloc and parsed.path:
                self.paths.add(parsed.path)


def main() -> int:
    site = Path(sys.argv[1] if len(sys.argv) > 1 else "_site").resolve()
    index = site / "index.html"
    manifest_path = site / "manifest.json"
    if not index.is_file() or not manifest_path.is_file():
        raise SystemExit(f"Incomplete installer site: {site}")

    parser = References()
    parser.feed(index.read_text(encoding="utf-8"))
    missing = sorted(path for path in parser.paths if not (site / path).is_file())
    if missing:
        raise SystemExit(f"Missing HTML resources: {missing}")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    parts = [part for build in manifest["builds"] for part in build["parts"]]
    missing = sorted(part["path"] for part in parts if not (site / part["path"]).is_file())
    if missing:
        raise SystemExit(f"Missing firmware parts: {missing}")

    offsets = [part["offset"] for part in parts]
    if offsets != [0, 32768, 57344, 65536]:
        raise SystemExit(f"Unexpected ESP32-C3 flash offsets: {offsets}")

    ET.parse(site / "cableado.svg")
    module_pattern = re.compile(r'(?:from\s*|import\s*\()\s*["\'](\./[^"\']+\.js)["\']')
    for script in site.rglob("*.js"):
        text = script.read_text(encoding="utf-8")
        if "🎉" in text:
            raise SystemExit(f"Unprofessional success icon remains in {script}")
        for relative in module_pattern.findall(text):
            dependency = (script.parent / relative).resolve()
            if not dependency.is_file():
                raise SystemExit(f"Missing module {relative} imported by {script}")

    dialog = next((site / "vendor").rglob("install-dialog-*.js"))
    dialog_text = dialog.read_text(encoding="utf-8")
    if not dialog_text.startswith("/* Modified by Camper Level:") or '"✓"' not in dialog_text:
        raise SystemExit("ESP Web Tools modification notice or neutral success icon is missing")

    print(f"Web installer validated: {site}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
