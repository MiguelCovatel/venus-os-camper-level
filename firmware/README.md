# Firmware Camper Level Monitor

Firmware independiente para **ESP32-C3 SuperMini + MPU-6500/MPU-9250**. No incluye sensores de gas y usa el namespace NVS propio `camperlevel`.

## Conexión

| ESP32-C3 | MPU-6500/9250 |
|---|---|
| 3V3 | VCC |
| GND | GND |
| GPIO8 | SDA |
| GPIO9 | SCL |

Conecta `NCS/CS` a 3V3 para forzar el modo I2C. Si el breakout integra su propio pull-up puede quedar libre, pero la conexión explícita a 3V3 es la opción recomendada; no lo conectes a GND durante el uso I2C. `AD0` abierto/GND selecciona `0x68`; a 3V3 selecciona `0x69`.

## Primera configuración por web

Después de grabarlo, conecta el móvil a `CamperLevel-XXXXXX` con contraseña `camperlevel`. El
portal se abre automáticamente; si no, visita `http://192.168.4.1`. Desde allí se configuran
Wi-Fi, MQTT, medidas, orientación, sensibilidad, Nivel 0 y actualizaciones OTA.

Tras conectarlo a la red, abre `http://camper-level.local` con usuario `admin`. La contraseña
inicial es `camperlevel` y debe cambiarse durante la puesta en marcha.

El método recomendado de grabación es el instalador web publicado en GitHub Pages.

## Recuperación por USB

El binario público se entrega sin credenciales. Abre el monitor serie a 115200 baudios y envía líneas terminadas en Enter:

```text
SET wifi_ssid MiWifi
SET wifi_password MiClave
SET mqtt_server 192.168.1.10
SET mqtt_port 1883
SET mqtt_username usuario
SET mqtt_password clave
SHOW
REBOOT
```

Para un broker sin usuario usa `SET mqtt_username -` y `SET mqtt_password -`. Otros comandos: `HELP`, `ZERO` y `FACTORY_RESET CONFIRM`. El reset solo borra el namespace NVS `camperlevel`.

## Compilar

```powershell
pio run -e esp32-c3-supermini
pio run -e esp32-c3-supermini -t upload --upload-port COM16
pio device monitor --port COM16 --baud 115200
```

## MQTT

Base predeterminada: `camper/level` (cambiable con `SET mqtt_base_topic ...`).

- `availability`: `online`/`offline`, retained y usado como LWT.
- `data`: JSON agregado, no retained.
- `status`, `pitch`, `roll`, `stable`, `state`, `quality`.
- `wheels/fl`, `wheels/fr`, `wheels/rl`, `wheels/rr`.
- `diagnostic/uptime`, `diagnostic/rssi`.
- Entrada: `config/set/<key>`; respuesta: `config/ack`.

Cada valor individual es un sobre JSON con timestamp, uptime, secuencia, boot ID y estado. Para poner el nivel actual a cero publica un identificador entero nuevo en `camper/level/config/set/level_zero`. Las dimensiones y ajustes de nivel admiten los mismos nombres que la consola serie.

## Tests

Los tests cubren nivel, pitch/roll positivos y negativos, combinación de ejes, vías distintas,
normalización sin negativos, redondeo práctico, histéresis, tolerancias y máquina de estabilidad.

```powershell
pio test -e native
```

En Windows sin GCC instalado, el repositorio de desarrollo incluye `scripts/test-native.ps1`, que acepta la ruta de un compilador Zig mediante `-Zig`.
