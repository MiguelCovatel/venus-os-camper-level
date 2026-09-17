# Publicar una versión en GitHub

## Antes del primer repositorio público

1. Confirmar propietario y nombre (`MiguelCovatel/venus-os-camper-level`).
2. Confirmar que el fichero `LICENSE` contiene la licencia MIT aprobada.
3. Añadir un contacto privado a `SECURITY.md` o habilitar informes privados de vulnerabilidades.
4. Revisar que no haya SSID, claves, IP personales ni artefactos del prototipo.

## Validación

```sh
cd firmware
pio run -e esp32-c3-supermini
pio test -e native

cd ../venus/dbus-camper-level
python -m unittest discover -s tests -v

cd ../gui-v2/CamperLevel
python build_plugin.py
```

También hay que probar instalación limpia, pantalla local, Remote Console, persistencia de medidas,
nivel 0, pérdida de MQTT, actualización y desinstalación.

`ci.yml` valida cada push/PR. `release.yml` se activa con una etiqueta `v*` y publica:

- `camper-level-firmware.zip`
- `venus-os-camper-level-venus.tar.gz`
- archivos `.sha256`

Usa primero una candidata (`v1.0.0-rc1`). Los Releases incluyen `paho-mqtt` vendorizado para que
el GX pueda instalarse sin `pip` ni Internet; CI conserva sus avisos de licencia dentro del paquete.
El ZIP incluye `camper-level-factory.bin`, una imagen unificada para grabar desde `0x0`; el
`firmware.bin` de PlatformIO por sí solo es la aplicación y corresponde al offset `0x10000`.

## ESP Web Tools

Puede añadirse cuando el firmware tenga portal cautivo o configurador USB web. Un manifiesto solo
graba el binario y no resuelve Wi-Fi/MQTT; por eso aún no se presenta como método principal.
