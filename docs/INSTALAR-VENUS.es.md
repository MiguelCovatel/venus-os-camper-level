# Instalar y actualizar Venus OS

## Antes de instalar

En Venus OS activa una sola opción:

```text
Ajustes > Integraciones > Acceso MQTT
```

Camper Level utilizará el broker que ya viene incluido en Venus OS. No necesitas instalar
Mosquitto, crear usuarios ni conocer una contraseña MQTT.

## Desde un Release descargado

```sh
sha256sum -c venus-os-camper-level-venus.tar.gz.sha256
tar -xzf venus-os-camper-level-venus.tar.gz
cd venus-os-camper-level-venus
./install.sh
```

El instalador configura el daemon con `127.0.0.1:1883` y comprueba que el broker responda. Al
terminar recuerda qué debe escribirse en la web del ESP32: la IP local de Venus OS, puerto `1883`,
usuario y contraseña vacíos.

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

Es el método recomendado para la mayoría de usuarios. Desde un ordenador abre PowerShell o
Terminal, entra en Venus OS y ejecuta el instalador:

```sh
ssh root@IP_DE_VENUS
```

Una vez dentro de Venus OS:

```sh
curl -fsSL https://raw.githubusercontent.com/MiguelCovatel/venus-os-camper-level/main/scripts/install-venus.sh | sh
```

El script localiza el Release publicado más reciente, aunque sea una versión candidata, verifica
SHA-256 y ejecuta el instalador incluido.

## Broker externo (opcional y avanzado)

Solo si el usuario ya dispone de otro broker:

```sh
export CAMPER_LEVEL_MQTT_PASSWORD='contraseña'
./install.sh --mqtt-host 192.168.1.10 --mqtt-user usuario
unset CAMPER_LEVEL_MQTT_PASSWORD
```

Para volver una instalación existente al MQTT incluido en Venus OS:

```sh
./install.sh --use-venus-mqtt
```

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
