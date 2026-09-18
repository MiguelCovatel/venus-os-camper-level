# Diagnóstico

## El IMU no aparece

1. Comprueba 3V3 y masa común.
2. Comprueba GPIO8→SDA y GPIO9→SCL.
3. Une NCS/CS a 3V3.
4. Prueba AD0 a GND (`0x68`) y vuelve a escanear.
5. Acorta cables y aléjalos de fuentes conmutadas.

El monitor debe indicar MPU-6500 o MPU-9250; no basta con que el ESP32 arranque.

## Venus muestra OFFLINE

1. Comprueba que `Ajustes > Integraciones > Acceso MQTT` esté activado.
2. En el ESP32, verifica que el servidor sea la IP local de Venus OS, puerto `1883`, sin usuario ni contraseña.
3. El daemon debe usar `127.0.0.1:1883` y el mismo `base_topic` (`camper/level`).
4. Comprueba `availability=online`, la edad del último mensaje y los logs del servicio.
5. Si elegiste voluntariamente un broker externo, verifica la ruta, credenciales y VPN desde ambos equipos.

Offline es intencionado: no se conserva la última inclinación como si siguiera siendo actual.

## Las medidas vuelven al valor anterior

Espera el ACK del ESP32. Si no llega, el daemon no debe presentar la edición como confirmada.
Comprueba permisos D-Bus, suscripción a `config/set/+` y NVS.

## El punto se mueve con la camper quieta

- Fija rígidamente el MPU.
- Aumenta moderadamente `stable_variation_deg` o `stable_duration_ms`.
- No calibres cero mientras haya personas moviéndose.
- Revisa alimentación USB y ruido I2C.
