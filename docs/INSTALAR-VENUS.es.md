# Instalar y actualizar Venus OS

## Desde un Release descargado

```sh
sha256sum -c venus-os-camper-level-venus.tar.gz.sha256
tar -xzf venus-os-camper-level-venus.tar.gz
cd venus-os-camper-level-venus
./install.sh --mqtt-host 192.168.1.10 --mqtt-user camper-level
```

El instalador solo debe escribir en:

```text
/data/apps/dbus-camper-level
/data/conf/camper-level
/service/dbus-camper-level
/data/apps/available/CamperLevel
/data/apps/enabled/CamperLevel
```

Si una ruta de servicio/plugin ya existe y no es el enlace esperado, se detiene en vez de
sobrescribirla. La configuración se conserva al actualizar.

Tras reiniciar o recargar GUI v2, la página se abre desde `Lista de dispositivos > Camper Level`.
También queda disponible la ruta de respaldo
`Ajustes > Integraciones > UI Plugins > CamperLevel`.

## Directamente desde GitHub

```sh
export CAMPER_LEVEL_REPOSITORY=MiguelCovatel/venus-os-camper-level
curl -fsSL "https://raw.githubusercontent.com/$CAMPER_LEVEL_REPOSITORY/main/scripts/install-venus.sh" \
  | sh -s -- --mqtt-host 192.168.1.10 --mqtt-user camper-level
```

El script descarga el Release y verifica SHA-256 antes de ejecutar el instalador incluido.

## Actualización

Ejecuta el mismo instalador con la nueva versión. Debe conservar:

- `/data/conf/camper-level/config.ini`
- `/data/conf/camper-level/environment`
- medidas y preferencias persistentes

## Desinstalación

```sh
./uninstall.sh
```

La desinstalación normal desactiva servicio y plugin conservando configuración. Solo `--purge`
puede borrar las carpetas específicas de CamperLevel.
