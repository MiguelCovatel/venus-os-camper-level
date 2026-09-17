# Inicio rápido

## 1. Montar el hardware

Conecta el MPU al ESP32-C3 usando 3V3, GND, GPIO8/SDA y GPIO9/SCL. Conecta `NCS/CS` a 3V3 para
forzar I2C. Consulta [CABLEADO.es.md](CABLEADO.es.md).

El sensor debe quedar rígido. No importa que la placa no esté perfectamente alineada con la
camper: al final se corrige con la orientación y el botón **Nivel 0**.

## 2. Grabar el ESP32

### Paquete de Release (recomendado)

1. Descarga `camper-level-firmware.zip`.
2. Instala Python y `esptool` una sola vez: `py -m pip install esptool`.
3. Conecta el ESP32 por USB.
4. En PowerShell, dentro del ZIP, ejecuta:

```powershell
.\flash-firmware.ps1 -Port COM16
```

Sustituye `COM16` por tu puerto. Una actualización normal no borra NVS y conserva configuración y
nivel 0. Usa `-Erase` solo para recuperar una configuración dañada.

### Compilar con PlatformIO

```powershell
cd firmware
pio run -e esp32-c3-supermini -t upload --upload-port COM16
pio device monitor --port COM16 --baud 115200
```

## 3. Configurar por USB

### Asistente recomendado

El ZIP incluye un asistente que pregunta los datos sin mostrar las contraseñas. Como `esptool`
ya instala `pyserial`, normalmente no hace falta añadir nada. Cierra antes cualquier monitor serie
y ejecuta:

```powershell
py .\configure-esp.py --port COM16
```

El asistente guarda Wi-Fi, MQTT y las tres medidas en el ESP32 y lo reinicia.

### Configuración manual

Abre el monitor serie a 115200 baudios. Escribe cada orden y pulsa Intro:

```text
SET wifi_ssid MiRed
SET wifi_password MiClave
SET mqtt_server 192.168.1.10
SET mqtt_port 1883
SET mqtt_username camper-level
SET mqtt_password OtraClave
SET mqtt_base_topic camper/level
SHOW
REBOOT
```

No compartas el resultado de `SHOW` si contiene datos privados. Para dejar usuario o contraseña
vacíos, usa `-` como valor cuando lo admita la ayuda `HELP` de la versión instalada.

## 4. Instalar en Venus OS

Descarga `venus-os-camper-level-venus.tar.gz`, verifica SHA-256, descomprímelo y ejecuta como root:

```sh
./install.sh --mqtt-host 192.168.1.10 --mqtt-user camper-level
```

El instalador solicita la contraseña sin mostrarla. Para instalación directa desde GitHub,
consulta [INSTALAR-VENUS.es.md](INSTALAR-VENUS.es.md).

## 5. Ajustar la camper

En Venus OS abre `Lista de dispositivos > Camper Level`. Si tu versión no muestra todavía esa
entrada, usa la ruta alternativa `Ajustes > Integraciones > UI Plugins > CamperLevel`:

1. Introduce batalla, vía delantera y vía trasera en milímetros, centro a centro.
2. Guarda y comprueba que persisten al salir y entrar.
3. Coloca la camper realmente nivelada con un nivel de referencia.
4. Espera a que deje de moverse y pulsa **Establecer nivel 0**.
5. Comprueba que pitch, roll y las cuatro ruedas se aproximan a cero.

La página indicará cuántos milímetros elevar en cada rueda. La rueda físicamente más alta siempre
se normaliza a `0 mm`, así que los valores principales nunca son negativos.
