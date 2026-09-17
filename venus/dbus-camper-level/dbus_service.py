"""D-Bus adapter exposing Camper Level to Venus OS and GUI v2."""

from __future__ import annotations

import importlib
import math
import os
import sys
from pathlib import Path
from typing import Any, Callable

from config import AppConfig
from mqtt_receiver import LevelZeroAck, ReceiverStats, Snapshot


def _load_vedbus_service() -> Any:
    candidates = [
        "/opt/victronenergy/velib_python",
        "/opt/victronenergy/dbus-systemcalc-py/ext/velib_python",
        "/data/velib_python",
        str(Path(__file__).resolve().parent / "vendor" / "velib_python"),
    ]
    for candidate in candidates:
        if os.path.isfile(os.path.join(candidate, "vedbus.py")) and candidate not in sys.path:
            sys.path.insert(0, candidate)
    try:
        return importlib.import_module("vedbus").VeDbusService
    except ImportError as exc:
        raise RuntimeError("vedbus.py not found; install on Venus OS or bundle velib_python") from exc


def _new_dbus_connection() -> Any:
    try:
        dbus = importlib.import_module("dbus")
    except ImportError as exc:
        raise RuntimeError("Python D-Bus bindings are unavailable") from exc
    bus_class = dbus.SessionBus if "DBUS_SESSION_BUS_ADDRESS" in os.environ else dbus.SystemBus
    return bus_class(private=True)


class DbusCamperLevelService:
    PRODUCT_ID = 0xC512
    LEVEL_SETTING_PATHS = {
        "/Settings/Level/WheelbaseMm": ("wheelbase_mm", 500, 12000),
        "/Settings/Level/FrontTrackMm": ("front_track_mm", 500, 4000),
        "/Settings/Level/RearTrackMm": ("rear_track_mm", 500, 4000),
    }
    NATIVE_SETTING_PATHS = {
        "/GenericInput/9/Value": ("wheelbase_mm", 500, 12000),
        "/GenericInput/10/Value": ("front_track_mm", 500, 4000),
        "/GenericInput/11/Value": ("rear_track_mm", 500, 4000),
    }
    SETTING_INPUTS = {"wheelbase_mm": 9, "front_track_mm": 10, "rear_track_mm": 11}
    PRIVATE_LIVE_PATHS = (
        "/Level/Pitch", "/Level/Roll", "/Level/Wheels/FrontLeft",
        "/Level/Wheels/FrontRight", "/Level/Wheels/RearLeft",
        "/Level/Wheels/RearRight", "/Level/RelativeHeight/FrontLeft",
        "/Level/RelativeHeight/FrontRight", "/Level/RelativeHeight/RearLeft",
        "/Level/RelativeHeight/RearRight",
    )
    NATIVE_INPUTS = (
        (0, "Pitch", "CAMPER LEVEL", 2, "°", 2, -89.0, 89.0, None),
        (1, "Roll", "CAMPER LEVEL", 2, "°", 2, -89.0, 89.0, None),
        (2, "Delantera izquierda", "CAMPER LEVEL", 2, "mm", 0, 0.0, 5000.0, None),
        (3, "Delantera derecha", "CAMPER LEVEL", 2, "mm", 0, 0.0, 5000.0, None),
        (4, "Trasera izquierda", "CAMPER LEVEL", 2, "mm", 0, 0.0, 5000.0, None),
        (5, "Trasera derecha", "CAMPER LEVEL", 2, "mm", 0, 0.0, 5000.0, None),
        (6, "Estado del nivel", "CAMPER LEVEL", 0, "", 0, None, None,
         ["NIVELADO", "ACEPTABLE", "DESNIVELADO", "SIN DATOS"]),
        (7, "Estabilidad", "CAMPER LEVEL", 0, "", 0, None, None,
         ["EN MOVIMIENTO", "ESTABILIZANDO", "ESTABLE", "SIN DATOS"]),
        (8, "ESP32", "CAMPER LEVEL", 0, "", 0, None, None, ["OFFLINE", "ONLINE"]),
        (9, "Batalla", "MEDIDAS", 2, "mm", 0, 500.0, 12000.0, None),
        (10, "Vía delantera", "MEDIDAS", 2, "mm", 0, 500.0, 4000.0, None),
        (11, "Vía trasera", "MEDIDAS", 2, "mm", 0, 500.0, 4000.0, None),
        (12, "Comunicación", "CAMPER LEVEL", 0, "", 0, None, None,
         ["NORMAL", "ADVERTENCIA", "OFFLINE"]),
        (13, "Sensor IMU", "CAMPER LEVEL", 0, "", 0, None, None,
         ["OK", "FALLO"]),
    )
    HIDDEN_INPUTS = (6, 7, 8, 9, 10, 11, 12, 13)

    def __init__(self, config: AppConfig,
                 level_setting_writer: Callable[[str, int], bool] | None = None,
                 level_zero_writer: Callable[[int], bool] | None = None,
                 initial_level_settings: dict[str, int] | None = None) -> None:
        self.config = config
        self._level_setting_writer = level_setting_writer
        self._level_zero_writer = level_zero_writer
        self._initial_settings = dict(initial_level_settings or {})
        service_class = _load_vedbus_service()
        self._service = service_class(config.service.dbus_name, register=False)
        self._native_bus = _new_dbus_connection()
        self._native_service = service_class(config.service.native_dbus_name,
                                             bus=self._native_bus, register=False)
        self._update_index = 0
        self._add_private_paths()
        self._add_native_paths()
        self._service.register()
        self._native_service.register()

    def _identity_paths(self, native: bool = False) -> dict[str, Any]:
        cfg = self.config
        return {
            "/Mgmt/ProcessName": os.path.abspath(sys.argv[0]),
            "/Mgmt/ProcessVersion": cfg.service.firmware_version,
            "/Mgmt/Connection": f"MQTT {cfg.mqtt.host}:{cfg.mqtt.port}",
            "/DeviceInstance": (cfg.service.native_device_instance if native else cfg.service.device_instance),
            "/ProductId": self.PRODUCT_ID,
            "/ProductName": cfg.service.product_name,
            "/CustomName": cfg.service.custom_name,
            "/Serial": cfg.service.serial,
            "/FirmwareVersion": cfg.service.firmware_version,
            "/HardwareVersion": "ESP32-C3 + MPU-6500/9250",
            "/Connected": 0,
        }

    def _add_private_paths(self) -> None:
        paths = self._identity_paths()
        paths.update({
            "/Level/Pitch": None, "/Level/Roll": None, "/Level/Stable": 0,
            "/Level/State": "OFFLINE", "/Level/Quality": "UNKNOWN",
            "/Level/ZeroStatus": "IDLE",
            "/Level/Wheels/FrontLeft": None, "/Level/Wheels/FrontRight": None,
            "/Level/Wheels/RearLeft": None, "/Level/Wheels/RearRight": None,
            "/Level/RelativeHeight/FrontLeft": None,
            "/Level/RelativeHeight/FrontRight": None,
            "/Level/RelativeHeight/RearLeft": None,
            "/Level/RelativeHeight/RearRight": None,
            "/Alarms/IMU": 0, "/Alarms/Communication": 2,
            "/Diagnostics/Uptime": None, "/Diagnostics/Rssi": None,
            "/Diagnostics/LastUpdate": None, "/Diagnostics/MessageAge": None,
            "/Diagnostics/Sequence": None, "/Diagnostics/BootId": "",
            "/Diagnostics/Timestamp": None, "/Diagnostics/TimeSynced": 0,
            "/Diagnostics/DataValid": 0, "/Diagnostics/SensorStatus": "OFFLINE",
            "/Diagnostics/InvalidMessages": 0,
            "/Diagnostics/OutOfOrderMessages": 0,
            "/Diagnostics/SequenceGaps": 0, "/Diagnostics/Reconnects": 0,
            "/UpdateIndex": 0,
        })
        for path, value in paths.items():
            self._service.add_path(path, value=value)
        for path, (key, _minimum, _maximum) in self.LEVEL_SETTING_PATHS.items():
            self._service.add_path(path, value=self._initial_settings.get(key),
                                   writeable=True,
                                   onchangecallback=self._on_setting_change)

    def _add_native_paths(self) -> None:
        paths = self._identity_paths(native=True)
        paths.update({"/State": 0, "/Mode": "MONITOR", "/OperatingState": "OFFLINE",
                      "/NrOfChannels": len(self.NATIVE_INPUTS), "/UpdateIndex": 0})
        for path, value in paths.items():
            self._native_service.add_path(path, value=value)
        for path, (key, _minimum, _maximum) in self.LEVEL_SETTING_PATHS.items():
            self._native_service.add_path(path, value=self._initial_settings.get(key),
                                          writeable=True,
                                          onchangecallback=self._on_setting_change)
        self._native_service.add_path("/Settings/Level/ZeroCommand", value=0,
                                      writeable=True,
                                      onchangecallback=self._on_zero_change)
        self._native_service.add_path("/Settings/Level/ZeroStatus", value=0)
        for index, name, group, input_type, unit, decimals, minimum, maximum, labels in self.NATIVE_INPUTS:
            base = f"/GenericInput/{index}"
            metadata = {
                f"{base}/Name": name, f"{base}/Settings/CustomName": "",
                f"{base}/Settings/Decimals": decimals,
                f"{base}/Settings/Group": group,
                f"{base}/Settings/ShowUIInput": 0 if index in self.HIDDEN_INPUTS else 1,
                f"{base}/Settings/Type": input_type,
                f"{base}/Settings/Unit": unit,
                f"{base}/Settings/ValidTypes": 1 << input_type,
            }
            if minimum is not None:
                metadata[f"{base}/Settings/RangeMin"] = minimum
            if maximum is not None:
                metadata[f"{base}/Settings/RangeMax"] = maximum
            if labels is not None:
                metadata[f"{base}/Settings/Labels"] = labels
            for path, value in metadata.items():
                self._native_service.add_path(path, value=value)
            setting = self.NATIVE_SETTING_PATHS.get(f"{base}/Value")
            if setting:
                initial = self._initial_settings.get(setting[0])
            elif index in (8, 12):
                initial = 0 if index == 8 else 2
            elif index == 13:
                initial = 1
            else:
                initial = len(labels) - 1 if labels else None
            self._native_service.add_path(f"{base}/Status", value=1)
            self._native_service.add_path(
                f"{base}/Value", value=initial, writeable=setting is not None,
                onchangecallback=self._on_setting_change if setting else None)

    def _on_setting_change(self, path: str, value: Any) -> bool:
        spec = self.LEVEL_SETTING_PATHS.get(path) or self.NATIVE_SETTING_PATHS.get(path)
        if spec is None or self._level_setting_writer is None or isinstance(value, bool):
            return False
        key, minimum, maximum = spec
        try:
            numeric = float(value)
        except (TypeError, ValueError, OverflowError):
            return False
        if not math.isfinite(numeric) or not minimum <= numeric <= maximum:
            return False
        accepted = bool(self._level_setting_writer(key, int(round(numeric))))
        if accepted:
            self._set_setting_values(key, int(round(numeric)))
        return accepted

    def _on_zero_change(self, _path: str, value: Any) -> bool:
        if self._level_zero_writer is None or isinstance(value, bool):
            return False
        try:
            request_id = int(value)
        except (TypeError, ValueError, OverflowError):
            return False
        if not 1 <= request_id <= 2_147_483_647:
            return False
        accepted = bool(self._level_zero_writer(request_id))
        if accepted:
            self._service["/Level/ZeroStatus"] = "PENDING"
            self._native_service["/Settings/Level/ZeroStatus"] = 1
        return accepted

    def _set_setting_values(self, key: str, value: Any) -> None:
        input_index = self.SETTING_INPUTS[key]
        private_path = next(path for path, spec in self.LEVEL_SETTING_PATHS.items() if spec[0] == key)
        self._service[private_path] = value
        self._native_service[private_path] = value
        self._native_service[f"/GenericInput/{input_index}/Value"] = value
        self._native_service[f"/GenericInput/{input_index}/Status"] = 0

    @staticmethod
    def _value(container: dict[str, Any], key: str) -> Any:
        value = container.get(key)
        return value if isinstance(value, (int, float, str)) and not isinstance(value, bool) else None

    def _native_input(self, index: int, value: Any, status: int = 0) -> None:
        self._native_service[f"/GenericInput/{index}/Value"] = value
        self._native_service[f"/GenericInput/{index}/Status"] = status

    def _bump(self) -> None:
        self._update_index = (self._update_index + 1) % 256
        self._service["/UpdateIndex"] = self._update_index
        self._native_service["/UpdateIndex"] = self._update_index

    @staticmethod
    def _mapped(value: Any, mapping: dict[str, int], fallback: int) -> int:
        return mapping.get(str(value).strip().upper(), fallback)

    def apply_level_zero_ack(self, acknowledgement: LevelZeroAck) -> None:
        self._native_service["/Settings/Level/ZeroCommand"] = acknowledgement.request_id
        self._native_service["/Settings/Level/ZeroStatus"] = 2 if acknowledgement.accepted else 3
        self._service["/Level/ZeroStatus"] = "SAVED" if acknowledgement.accepted else "ERROR"
        self._bump()

    def apply_snapshot(self, snapshot: Snapshot, stats: ReceiverStats,
                       desired_settings: dict[str, int] | None = None) -> None:
        data, level = snapshot.payload, snapshot.payload["level"]
        wheels = level["wheel_mm"]
        relative = level.get("relative_height_mm", {})
        imu = data["imu"]
        network = data["network"]
        valid = bool(level.get("valid")) and bool(imu.get("healthy", False))
        fault = 0 if valid else 1
        values = {
            "/Connected": 1,
            "/Level/Pitch": self._value(level, "pitch_deg") if valid else None,
            "/Level/Roll": self._value(level, "roll_deg") if valid else None,
            "/Level/Stable": int(bool(level.get("stable")) and valid),
            "/Level/State": str(level.get("state", "UNKNOWN")) if valid else "FAULT",
            "/Level/Quality": str(level.get("quality", "UNKNOWN")) if valid else "UNKNOWN",
            "/Level/Wheels/FrontLeft": self._value(wheels, "fl") if valid else None,
            "/Level/Wheels/FrontRight": self._value(wheels, "fr") if valid else None,
            "/Level/Wheels/RearLeft": self._value(wheels, "rl") if valid else None,
            "/Level/Wheels/RearRight": self._value(wheels, "rr") if valid else None,
            "/Level/RelativeHeight/FrontLeft": self._value(relative, "fl") if valid else None,
            "/Level/RelativeHeight/FrontRight": self._value(relative, "fr") if valid else None,
            "/Level/RelativeHeight/RearLeft": self._value(relative, "rl") if valid else None,
            "/Level/RelativeHeight/RearRight": self._value(relative, "rr") if valid else None,
            "/Alarms/IMU": 2 if fault else 0,
            "/Alarms/Communication": 0,
            "/Diagnostics/Uptime": int(data["uptime_ms"]) / 1000.0,
            "/Diagnostics/Rssi": self._value(network, "rssi_dbm"),
            "/Diagnostics/LastUpdate": snapshot.received_wall_time,
            "/Diagnostics/MessageAge": 0.0,
            "/Diagnostics/Sequence": int(data["sequence"]),
            "/Diagnostics/BootId": str(data["boot_id"]),
            "/Diagnostics/Timestamp": data.get("timestamp_ms"),
            "/Diagnostics/TimeSynced": int(bool(data.get("time_synced"))),
            "/Diagnostics/DataValid": int(valid),
            "/Diagnostics/SensorStatus": str(data.get("sensor_status", "UNKNOWN")),
        }
        for path, value in values.items():
            self._service[path] = value
        self._apply_stats(stats)

        self._native_service["/Connected"] = 1
        self._native_service["/State"] = 256
        self._native_service["/OperatingState"] = "NORMAL" if valid else "SENSOR FAULT"
        self._native_input(0, self._value(level, "pitch_deg") if valid else None, fault)
        self._native_input(1, self._value(level, "roll_deg") if valid else None, fault)
        for index, key in ((2, "fl"), (3, "fr"), (4, "rl"), (5, "rr")):
            self._native_input(index, self._value(wheels, key) if valid else None, fault)
        quality = self._mapped(level.get("quality"), {
            "LEVEL": 0, "PERFECT": 0, "ACCEPTABLE": 1,
            "SLIGHTLY_UNLEVEL": 1, "UNLEVEL": 2}, 3)
        stability = self._mapped(level.get("state"), {
            "MOVING": 0, "STABILIZING": 1, "STABLE": 2}, 3)
        self._native_input(6, quality if valid else 3, fault)
        self._native_input(7, stability if valid else 3, fault)
        self._native_input(8, 1, 0)
        self._native_input(12, 0, 0)
        self._native_input(13, fault, 0)

        dimensions = level.get("dimensions_mm", {})
        names = {"wheelbase_mm": "wheelbase", "front_track_mm": "front_track",
                 "rear_track_mm": "rear_track"}
        overrides = desired_settings or {}
        for key, telemetry_key in names.items():
            value = overrides.get(key, self._value(dimensions, telemetry_key))
            if value is not None:
                self._set_setting_values(key, value)
        self._bump()

    def _apply_stats(self, stats: ReceiverStats) -> None:
        self._service["/Diagnostics/InvalidMessages"] = stats.invalid_messages
        self._service["/Diagnostics/OutOfOrderMessages"] = stats.out_of_order_messages
        self._service["/Diagnostics/SequenceGaps"] = stats.sequence_gaps
        self._service["/Diagnostics/Reconnects"] = stats.reconnects

    def apply_communication_state(self, message_age: float | None, severity: int,
                                  stats: ReceiverStats) -> None:
        self._service["/Diagnostics/MessageAge"] = message_age
        self._service["/Alarms/Communication"] = severity
        self._apply_stats(stats)
        self._native_input(12, severity, 0 if severity == 0 else 1)
        if severity == 1:
            self._native_service["/OperatingState"] = "COMMUNICATION WARNING"
        elif severity >= 2:
            self._service["/Connected"] = 0
            self._service["/Diagnostics/DataValid"] = 0
            self._service["/Level/Stable"] = 0
            self._service["/Level/State"] = "OFFLINE"
            self._service["/Level/Quality"] = "UNKNOWN"
            for path in self.PRIVATE_LIVE_PATHS:
                self._service[path] = None
            self._native_service["/Connected"] = 0
            self._native_service["/State"] = 0
            self._native_service["/OperatingState"] = "OFFLINE"
            for index in range(0, 8):
                self._native_input(index, 3 if index in (6, 7) else None, 1)
            self._native_input(8, 0, 1)
            self._native_input(13, 1, 1)
        self._bump()
