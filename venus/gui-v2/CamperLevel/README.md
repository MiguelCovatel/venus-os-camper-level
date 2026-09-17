# CamperLevel GUI v2 plugin

Build with `python build_plugin.py`. Install the resulting
`build/CamperLevel.json` with `install.sh` on Venus OS.

The plugin registers both a Settings/Integrations fallback and a device-detail
page for ProductId `0xC512`. It consumes the native switch service instance 42,
which works on the local GX display and through Remote Console.
