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

1. Descarga `camper-level-firmware.zip` desde **Releases**.
2. Graba el ESP32 por USB y ejecuta el asistente `configure-esp.py` para introducir Wi-Fi, MQTT y medidas sin recompilar.
3. Descarga el paquete Venus OS y ejecuta su único `install.sh`.
4. Abre `Lista de dispositivos > Camper Level` y pulsa **Nivel 0** con la camper nivelada.
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
ESP32-C3 ── Wi-Fi/MQTT ──► broker ──► dbus-camper-level ──► D-Bus
                                                              │
                                                              ▼
                                                     Venus OS GUI v2
```

El ESP32 no necesita conexión directa con la Raspberry Pi/GX. Solo el ESP32 y Venus OS deben
poder alcanzar el mismo broker MQTT. Más detalles en
[Arquitectura](docs/ARQUITECTURA.md) y [MQTT](docs/MQTT.md).

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
