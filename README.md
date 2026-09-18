# Camper Level para Venus OS

Nivelador comunitario para campers y autocaravanas basado en un **ESP32-C3 SuperMini** y un
**MPU-6500 o MPU-9250**. Calcula `pitch`, `roll` y los milímetros que hay que elevar en cada
rueda, y muestra el resultado en Venus OS GUI v2.

Esta es la edición independiente de nivel; no incluye sensores de gases.

## Qué ofrece

- Autodetección de MPU-6500/MPU-9250 por I2C.
- Filtrado y estados `MOVING`, `STABILIZING` y `STABLE`.
- Corrección de las cuatro ruedas (`FL`, `FR`, `RL`, `RR`) sin valores negativos.
- Batalla y vías configurables desde Venus OS.
- Botón **Establecer nivel 0** con offsets persistentes en el ESP32.
- Orientación adaptable mediante intercambio e inversión de ejes.
- MQTT con secuencia, `boot_id`, uptime, estado del IMU y LWT.
- Desconexión fail-safe: Venus OS invalida los datos antiguos.
- Página gráfica GUI v2, local y en Remote Console.
- Instalación del firmware desde el navegador, sin Python ni PlatformIO.
- Portal web móvil para Wi-Fi, MQTT, medidas, orientación, nivel cero y OTA.
- Presentación práctica tipo burbuja: décimas de grado y correcciones de 10 mm.
- Simulador y tests para desarrollar sin hardware.

## Hardware mínimo

- ESP32-C3 SuperMini.
- Módulo MPU-6500 o MPU-9250.
- Cuatro o cinco cables cortos y alimentación USB estable.

| ESP32-C3 | MPU-6500/9250 | Función |
|---|---|---|
| 3V3 | VCC | Alimentación a 3,3 V |
| GND | GND | Masa común |
| GPIO8 | SDA | Datos I2C |
| GPIO9 | SCL | Reloj I2C |
| 3V3 | NCS/CS | Fuerza el modo I2C |

`AD0` a GND selecciona `0x68`; a 3V3 selecciona `0x69`. Ambas direcciones se detectan.
Consulta [el esquema completo](docs/CABLEADO.es.md) antes de soldar.

## Instalación rápida

1. Abre el [instalador web](https://miguelcovatel.github.io/venus-os-camper-level/) con Chrome o Edge y graba el ESP32 por USB.
2. En Venus OS activa `Ajustes > Integraciones > Acceso MQTT`.
3. Conecta el móvil a `CamperLevel-XXXXXX` con la clave `camperlevel`; solo necesitarás la Wi-Fi, la IP de Venus OS y las medidas.
4. Descarga el paquete Venus OS y ejecuta su único `install.sh`, sin parámetros MQTT.
5. Abre `Lista de dispositivos > Camper Level` y pulsa **Nivel 0** con la camper nivelada.
   Como acceso alternativo, usa `Ajustes > Integraciones > UI Plugins > CamperLevel`.

La guía completa está en [Inicio rápido](docs/INICIO-RAPIDO.es.md).

Repositorio previsto: `MiguelCovatel/venus-os-camper-level`.

## Estructura

```text
firmware/                       PlatformIO, ESP32-C3 + IMU
venus/dbus-camper-level/        Receptor MQTT y servicios D-Bus
venus/gui-v2/CamperLevel/       Plugin gráfico GUI v2
docs/                           Cableado, instalación y diagnóstico
scripts/                        Empaquetado, grabación e instalación
.github/workflows/              CI y generación de Releases
```

## Arquitectura

```text
MPU-6500/9250
      │ I2C
      ▼
ESP32-C3 ── Wi-Fi/MQTT ──► MQTT incluido en Venus OS ──► dbus-camper-level ──► D-Bus
                                                              │
                                                              ▼
                                                     Venus OS GUI v2
```

El modo recomendado no necesita CasaOS, Home Assistant ni otro broker: utiliza el MQTT que ya
incluye Venus OS. En la web del ESP32 se introduce la IP local del GX/Raspberry Pi, puerto `1883`,
sin usuario ni contraseña. El broker externo se conserva únicamente como opción avanzada. Más
detalles en [Arquitectura](docs/ARQUITECTURA.md) y [MQTT](docs/MQTT.md).

Una vez configurado, el panel del ESP32 está en `http://camper-level.local` con usuario `admin`.
La grabación por PowerShell y el asistente USB siguen disponibles como método de recuperación.

## Desarrollo

```sh
cd firmware
pio run -e esp32-c3-supermini
pio test -e native

cd ../venus/dbus-camper-level
python -m unittest discover -s tests -v

cd ../gui-v2/CamperLevel
python build_plugin.py
```

Consulta [Publicar una versión](docs/PUBLICAR.md) para generar los artefactos.

## Licencia

Publicado bajo licencia [MIT](LICENSE).
