#!/usr/bin/env python3
"""Interactive USB configuration wizard for Camper Level firmware."""

from __future__ import annotations

import argparse
import getpass
import sys
import time
from typing import Any


def ask(prompt: str, default: str = "", required: bool = False) -> str:
    suffix = f" [{default}]" if default else ""
    while True:
        value = input(f"{prompt}{suffix}: ").strip()
        if not value:
            value = default
        if value or not required:
            return value
        print("Este campo es obligatorio.")


def ask_password(prompt: str, required: bool = False) -> str:
    while True:
        value = getpass.getpass(f"{prompt}: ")
        if value or not required:
            return value
        print("Este campo es obligatorio.")


def ask_integer(prompt: str, default: int, minimum: int, maximum: int) -> int:
    while True:
        text = ask(prompt, str(default))
        try:
            value = int(text)
        except ValueError:
            print("Introduce un número entero.")
            continue
        if minimum <= value <= maximum:
            return value
        print(f"El valor debe estar entre {minimum} y {maximum}.")


def safe_value(value: str, maximum: int) -> str:
    if "\r" in value or "\n" in value:
        raise ValueError("Los valores no pueden contener saltos de línea.")
    if len(value.encode("utf-8")) > maximum:
        raise ValueError(f"El valor supera {maximum} caracteres.")
    return value if value else "-"


def send_setting(port: Any, key: str, value: str, timeout_s: float = 4.0) -> None:
    command = f"SET {key} {value}"
    port.write((command + "\n").encode("utf-8"))
    port.flush()
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        raw = port.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").strip()
        if line == "OK saved":
            print(f"  OK  {key}")
            return
        if line.startswith("ERROR "):
            raise RuntimeError(f"{key}: {line}")
    raise TimeoutError(f"El ESP32 no confirmó {key}. Comprueba puerto y firmware.")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Configura Camper Level por USB sin recompilar el firmware."
    )
    parser.add_argument("--port", required=True, help="Puerto serie, por ejemplo COM16")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        print("Falta pyserial. Ejecuta: py -m pip install pyserial", file=sys.stderr)
        return 2

    print("Camper Level — configuración por USB")
    print("Las contraseñas no se mostrarán ni se guardarán en este ordenador.\n")
    ssid = ask("Nombre de la red Wi-Fi (2,4 GHz)", required=True)
    wifi_password = ask_password("Contraseña Wi-Fi")
    mqtt_server = ask("Dirección IP o nombre del broker MQTT", required=True)
    mqtt_port = ask_integer("Puerto MQTT", 1883, 1, 65535)
    mqtt_username = ask("Usuario MQTT (vacío si no se usa)")
    mqtt_password = ask_password("Contraseña MQTT (vacía si no se usa)")
    base_topic = ask("Topic base MQTT", "camper/level", required=True)
    device_name = ask("Nombre del dispositivo", "camper-level", required=True)
    wheelbase = ask_integer("Batalla en mm", 3200, 500, 12000)
    front_track = ask_integer("Vía delantera centro a centro en mm", 1800, 500, 4000)
    rear_track = ask_integer("Vía trasera centro a centro en mm", 1800, 500, 4000)

    try:
        settings = (
            ("wifi_ssid", safe_value(ssid, 32)),
            ("wifi_password", safe_value(wifi_password, 64)),
            ("mqtt_server", safe_value(mqtt_server, 64)),
            ("mqtt_port", str(mqtt_port)),
            ("mqtt_username", safe_value(mqtt_username, 64)),
            ("mqtt_password", safe_value(mqtt_password, 64)),
            ("mqtt_base_topic", safe_value(base_topic, 64)),
            ("device_name", safe_value(device_name, 32)),
            ("wheelbase_mm", str(wheelbase)),
            ("front_track_mm", str(front_track)),
            ("rear_track_mm", str(rear_track)),
        )
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print(f"\nConectando a {args.port}…")
    try:
        with serial.Serial(args.port, args.baud, timeout=0.25) as port:
            time.sleep(1.5)
            port.reset_input_buffer()
            for key, value in settings:
                send_setting(port, key, value)
            port.write(b"REBOOT\n")
            port.flush()
    except (OSError, ValueError, RuntimeError, TimeoutError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print("\nConfiguración guardada. El ESP32 se está reiniciando.")
    print("Después podrás cambiar medidas y guardar NIVEL 0 desde Venus OS.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
