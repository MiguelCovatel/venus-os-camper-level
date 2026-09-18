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

El workflow `pages.yml` compila y publica el instalador web, su croquis, la copia local de ESP Web
Tools, el manifiesto y los cuatro binarios en GitHub Pages. Después de grabar, el portal cautivo
del ESP32 resuelve Wi-Fi, IP de Venus OS, medidas, orientación y Nivel 0, por lo que ESP Web Tools
es el método principal.

El contador es deliberadamente orientativo. `installer.js` consulta el contador sin aumentarlo al
abrir la página y usa el endpoint `hit` únicamente cuando la herramienta muestra
`Installation complete!`. Una caída del contador nunca debe bloquear la grabación. No debe
añadirse SSID, IP de Venus, número de serie ni otra configuración al evento.

La copia vendorizada corresponde a `esp-web-tools@10.4.0`. Al actualizarla hay que conservar la
licencia Apache-2.0, revisar `README-CAMPER-LEVEL.md`, volver a aplicar el icono neutro y comprobar
que el detector de finalización sigue coincidiendo con el texto de la nueva versión.

Antes de etiquetar una versión hay que verificar que la página, `manifest.json`, `firmware.bin`,
`bootloader.bin`, `partitions.bin` y `boot_app0.bin` respondan correctamente.
