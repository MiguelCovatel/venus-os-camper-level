"""Configuration for the standalone Camper Level Venus OS service."""

from __future__ import annotations

import configparser
import os
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class MqttConfig:
    host: str
    port: int
    username: str
    password: str
    base_topic: str
    client_id: str
    keepalive_s: int


@dataclass(frozen=True)
class ServiceConfig:
    dbus_name: str
    device_instance: int
    native_dbus_name: str
    native_device_instance: int
    product_name: str
    custom_name: str
    serial: str
    firmware_version: str
    warning_timeout_s: float
    critical_timeout_s: float
    message_max_age_s: float
    future_tolerance_s: float
    level_settings_file: str
    log_level: str


@dataclass(frozen=True)
class AppConfig:
    mqtt: MqttConfig
    service: ServiceConfig

    @classmethod
    def load(cls, filename: str | Path) -> "AppConfig":
        path = Path(filename)
        parser = configparser.ConfigParser(interpolation=None)
        if not parser.read(path, encoding="utf-8"):
            raise ValueError(f"configuration file not found: {path}")
        mqtt = parser["mqtt"]
        service = parser["service"]
        timeouts = parser["timeouts"]
        password_env = mqtt.get("password_env", "CAMPER_LEVEL_MQTT_PASSWORD").strip()
        password = os.environ.get(password_env, mqtt.get("password", ""))
        result = cls(
            mqtt=MqttConfig(
                host=mqtt.get("host", "127.0.0.1").strip(),
                port=mqtt.getint("port", 1883),
                username=mqtt.get("username", "").strip(),
                password=password,
                base_topic=mqtt.get("base_topic", "camper/level").strip().strip("/"),
                client_id=mqtt.get("client_id", "venus-camper-level").strip(),
                keepalive_s=mqtt.getint("keepalive_s", 30),
            ),
            service=ServiceConfig(
                dbus_name=service.get("dbus_name", "com.victronenergy.camperlevel").strip(),
                device_instance=service.getint("device_instance", 42),
                native_dbus_name=service.get("native_dbus_name", "com.victronenergy.switch.camperlevel").strip(),
                native_device_instance=service.getint("native_device_instance", 42),
                product_name=service.get("product_name", "Camper Level").strip(),
                custom_name=service.get("custom_name", "Camper Level").strip(),
                serial=service.get("serial", "camper-level").strip(),
                firmware_version=service.get("firmware_version", "1.0.0").strip(),
                warning_timeout_s=timeouts.getfloat("warning_s", 10.0),
                critical_timeout_s=timeouts.getfloat("critical_s", 30.0),
                message_max_age_s=timeouts.getfloat("message_max_age_s", 30.0),
                future_tolerance_s=timeouts.getfloat("future_tolerance_s", 60.0),
                level_settings_file=service.get("level_settings_file", "/data/conf/camper-level/level-settings.json").strip(),
                log_level=service.get("log_level", "INFO").strip().upper(),
            ),
        )
        cls._validate(result)
        return result

    @staticmethod
    def _validate(config: "AppConfig") -> None:
        mqtt, service = config.mqtt, config.service
        if not mqtt.host:
            raise ValueError("mqtt.host must not be empty")
        if not 1 <= mqtt.port <= 65535:
            raise ValueError("mqtt.port must be between 1 and 65535")
        if not mqtt.base_topic:
            raise ValueError("mqtt.base_topic must not be empty")
        if mqtt.username and not mqtt.password:
            raise ValueError("mqtt.password or its environment override is required")
        if "replace-me" in {mqtt.username, mqtt.password}:
            raise ValueError("replace the example MQTT credentials before installation")
        if mqtt.keepalive_s < 5:
            raise ValueError("mqtt.keepalive_s must be at least 5")
        if service.warning_timeout_s <= 0:
            raise ValueError("timeouts.warning_s must be positive")
        if service.critical_timeout_s <= service.warning_timeout_s:
            raise ValueError("timeouts.critical_s must be greater than warning_s")
        if service.message_max_age_s <= 0 or service.future_tolerance_s < 0:
            raise ValueError("invalid timestamp tolerances")
        if not 0 <= service.device_instance <= 255 or not 0 <= service.native_device_instance <= 255:
            raise ValueError("device instances must be between 0 and 255")
        if not service.dbus_name.startswith("com.victronenergy."):
            raise ValueError("service.dbus_name must start with com.victronenergy.")
        if not service.native_dbus_name.startswith("com.victronenergy.switch."):
            raise ValueError("native_dbus_name must start with com.victronenergy.switch.")
        if service.dbus_name == service.native_dbus_name:
            raise ValueError("private and native D-Bus names must be different")
