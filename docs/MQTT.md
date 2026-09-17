# Contrato MQTT

El topic base por defecto es `camper/level`. Si hay varios dispositivos, usa un topic distinto
para cada uno, por ejemplo `camper/mi-furgo/level`.

## Disponibilidad

```text
camper/level/availability = online | offline
```

Se publica `online` al conectar y se configura un Last Will retenido con `offline`. La telemetría
viva no se considera válida por el mero hecho de estar retenida.

## Telemetría agregada

```text
camper/level/data
```

Ejemplo orientativo:

```json
{
  "schema_version": 1,
  "firmware_version": "1.1.0",
  "timestamp_ms": 1780000000000,
  "time_synced": true,
  "sample_time_ms": 125400,
  "uptime_ms": 125400,
  "sequence": 45672,
  "boot_id": "A1B2C3D4E5F60708",
  "device_name": "camper-level",
  "sensor_status": "OK",
  "imu": {
    "type": "MPU-6500",
    "address": 104,
    "who_am_i": 112,
    "healthy": true,
    "fault": "none"
  },
  "level": {
    "valid": true,
    "pitch_deg": 0.35,
    "roll_deg": -0.18,
    "state": "STABLE",
    "stable": true,
    "quality": "ACCEPTABLE",
    "dimensions_mm": {"wheelbase": 3600, "front_track": 1750, "rear_track": 1750},
    "wheel_mm": {"fl": 25, "fr": 10, "rl": 15, "rr": 0},
    "relative_height_mm": {"fl": -5, "fr": 10, "rl": 5, "rr": 20}
  },
  "network": {
    "state": "ONLINE",
    "rssi_dbm": -58,
    "mqtt_error": 0,
    "last_publish_ms": 124400,
    "publish_failures": 0
  },
  "diagnostics": {"reset_reason": "POWERON_RESET"}
}
```

También se publican topics individuales: `status`, `pitch`, `roll`, `stable`, `state`, `quality`,
`wheels/fl`, `wheels/fr`, `wheels/rl`, `wheels/rr`, `diagnostic/uptime` y `diagnostic/rssi`.
El esquema exacto de cada versión es el que validan los tests del daemon.

Desde firmware `1.1`, `wheel_mm` y los topics `wheels/*` contienen la corrección práctica
redondeada a `10 mm`. `relative_height_mm` conserva la geometría sin redondear para diagnóstico.

## Configuración remota

El daemon publica con QoS 1 y sin `retain`:

```text
camper/level/config/set/wheelbase_mm
camper/level/config/set/front_track_mm
camper/level/config/set/rear_track_mm
camper/level/config/set/level_zero
```

El ESP valida, guarda y responde en `camper/level/config/ack`. No debe aceptarse una medida fuera
de límites ni interpretarse un ACK antiguo como confirmación de una orden actual.

Cada lote incluye `sequence`, `boot_id` y `uptime_ms`. La secuencia solo se compara dentro del
mismo `boot_id`; un cambio de `boot_id` representa un reinicio válido. Con NTP, `timestamp_ms`
permite rechazar mensajes caducados; sin NTP se usa el momento local de recepción.
