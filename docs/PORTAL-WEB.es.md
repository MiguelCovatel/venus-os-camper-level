# Portal web del ESP32

## Primera configuración

Sin una configuración válida, el ESP32 crea `CamperLevel-XXXXXX` con contraseña `camperlevel`.
El portal cautivo dirige a `http://192.168.4.1` y permite configurar:

- Wi-Fi de 2,4 GHz.
- Broker, puerto y credenciales MQTT.
- Nombre y topic del dispositivo.
- Batalla y vías.
- Perfil de sensibilidad y orientación del MPU.
- Contraseña de administración de la web.

Al guardar se valida el conjunto completo antes de escribirlo en NVS. Después el ESP32 reinicia.

## Acceso habitual

En la misma red abre `http://camper-level.local`. El navegador solicita usuario `admin` y la
contraseña elegida. La contraseña inicial es `camperlevel`; se recomienda cambiarla.

La web ofrece lecturas, Nivel 0, configuración, reinicio y OTA. Las contraseñas guardadas nunca se
devuelven al formulario. Si se dejan vacías se conservan las existentes.

## Recuperación

Si no logra conectarse a la Wi-Fi durante dos minutos, el ESP32 vuelve a abrir su punto de acceso.
También permanecen disponibles la consola serie y `configure-esp.py` como métodos de recuperación.

Una actualización OTA normal conserva NVS. Un borrado completo mediante el instalador USB elimina
Wi-Fi, MQTT, medidas y Nivel 0.
