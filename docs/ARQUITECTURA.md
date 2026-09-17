# Arquitectura

## ESP32-C3

- Detecta e inicializa el MPU.
- Filtra acelerómetro y giroscopio para obtener pitch/roll.
- Aplica orientación y offsets persistentes.
- Determina movimiento, estabilización y estabilidad.
- Calcula altura relativa y elevación normalizada por rueda.
- Guarda configuración en NVS.
- Publica telemetría y disponibilidad por MQTT.
- Recibe medidas y orden de nivel 0.
- Reinicia tareas bloqueadas mediante watchdog.

## Venus OS

- Recibe MQTT y valida formato, secuencia y frescura.
- No mantiene como válidas lecturas antiguas al perder comunicación.
- Publica un servicio D-Bus detallado y un adaptador nativo para GUI v2.
- Permite cambiar batalla/vías y solicitar nivel 0.
- Presenta la camper y las cuatro correcciones en GUI v2.

## Configuración

```text
GUI v2 ─► D-Bus escribible ─► daemon ─► MQTT config/set ─► ESP32 ─► NVS
                                                        │
                                                        └─► config/ack
```

La GUI no calcula alturas. El cálculo vive en el ESP32 para que todos los consumidores obtengan
el mismo resultado. Venus conserva las medidas para mostrarlas y reenviarlas, pero la confirmación
del ESP32 es la autoridad final.

## Nombres aislados

- Topic: `camper/level`
- D-Bus privado: `com.victronenergy.camperlevel`
- D-Bus nativo: `com.victronenergy.switch.camperlevel`
- Device instance nativa: `42`
- Product ID: `0xC512`

