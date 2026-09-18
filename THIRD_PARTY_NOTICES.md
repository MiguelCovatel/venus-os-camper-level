# Third-party notices

Release bundles vendor `paho-mqtt` so Venus OS can be installed offline. The upstream package,
its metadata and its license files are copied unchanged into the daemon's `vendor` directory by
the release workflow. Do not remove those files when redistributing a bundle.

PlatformIO, the ESP32 Arduino framework, PubSubClient, Unity, Qt/PySide and GitHub Actions are
build/test dependencies and are not represented as original project code. Their respective
licenses continue to apply.

The browser installer vendors ESP Web Tools 10.4.0 from the Open Home Foundation under the
Apache License 2.0. Its license is included at
`web-installer/vendor/esp-web-tools-10.4.0/LICENSE`. The distributed install-dialog bundle is
modified only to replace the upstream party-popper success icon with a neutral check mark; the
modified file carries a notice at its beginning.
