# Portal web del ESP32

## Primera configuración

Sin una configuración válida, el ESP32 crea `CamperLevel-XXXXXX` con contraseña `camperlevel`.
El portal cautivo dirige a `http://192.168.4.1` y permite configurar:

- Wi-Fi de 2,4 GHz.
- IP local de Venus OS. El puerto y credenciales MQTT quedan ocultos como opciones avanzadas.
- Nombre y topic del dispositivo.
- Batalla y vías.
- Perfil de sensibilidad y orientación del MPU.
- Contraseña de administración de la web.

Al guardar se valida el conjunto completo antes de escribirlo en NVS. Después el ESP32 reinicia.

## Conexión recomendada con Venus OS

1. Activa `Ajustes > Integraciones > Acceso MQTT` en Venus OS.
2. Busca su IP en `Ajustes > Conectividad > Wi-Fi/Ethernet`.
3. Escribe esa IP en **IP de Venus OS**.

No hace falta crear usuario ni contraseña: deja el puerto `1883` y el topic `camper/level`. Los
campos avanzados solo se utilizan cuando el usuario decide instalar su propio broker externo.

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
