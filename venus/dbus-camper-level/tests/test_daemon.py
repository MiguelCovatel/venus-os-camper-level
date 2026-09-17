from __future__ import annotations

import json
import os
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

import dbus_service as dbus_module
from config import AppConfig, MqttConfig, ServiceConfig
from dbus_service import DbusCamperLevelService
from main import communication_severity
from mqtt_receiver import (LevelZeroAck, MessageValidationError, MqttReceiver,
                           ReceiverStats, Snapshot, TelemetryValidator)


def telemetry(sequence: int = 1, boot_id: str = "BOOT1",
              now: float | None = None, valid: bool = True) -> dict[str, object]:
    timestamp = time.time() if now is None else now
    numeric = {"fl": 12.0, "fr": 5.0, "rl": 7.0, "rr": 0.0}
    wheels = numeric if valid else {key: None for key in numeric}
    return {
        "schema_version": 1, "timestamp_ms": int(timestamp * 1000),
        "time_synced": True, "uptime_ms": 12000, "sequence": sequence,
        "boot_id": boot_id, "device_name": "camper-level-test",
        "sensor_status": "OK" if valid else "IMU_FAULT",
        "imu": {"type": "MPU-6500", "who_am_i": 112, "healthy": valid},
        "level": {
            "valid": valid, "pitch_deg": 0.2 if valid else None,
            "roll_deg": -0.1 if valid else None, "state": "STABLE" if valid else "FAULT",
            "stable": valid, "quality": "ACCEPTABLE" if valid else "UNKNOWN",
            "dimensions_mm": {"wheelbase": 3200.0, "front_track": 1800.0,
                              "rear_track": 1780.0},
            "wheel_mm": wheels,
            "relative_height_mm": {"fl": -6.0, "fr": 1.0, "rl": -1.0, "rr": 6.0},
        },
        "network": {"state": "ONLINE", "rssi_dbm": -62},
    }


class ValidatorTests(unittest.TestCase):
    def test_valid_sequence_and_gap(self) -> None:
        validator = TelemetryValidator(30, 60)
        now = time.time()
        validator.validate(json.dumps(telemetry(1, now=now)), now)
        validator.validate(json.dumps(telemetry(3, now=now)), now)
        self.assertEqual(validator.sequence_gaps, 1)

    def test_duplicate_stale_and_future_are_rejected(self) -> None:
        validator = TelemetryValidator(30, 60)
        now = time.time()
        validator.validate(json.dumps(telemetry(1, now=now)), now)
        with self.assertRaisesRegex(MessageValidationError, "duplicate"):
            validator.validate(json.dumps(telemetry(1, now=now)), now)
        with self.assertRaisesRegex(MessageValidationError, "stale"):
            TelemetryValidator(30, 60).validate(json.dumps(telemetry(now=now - 31)), now)
        with self.assertRaisesRegex(MessageValidationError, "future"):
            TelemetryValidator(30, 60).validate(json.dumps(telemetry(now=now + 61)), now)

    def test_unsynchronized_boot_message_with_null_timestamp_is_accepted(self) -> None:
        data = telemetry()
        data["time_synced"] = False
        data["timestamp_ms"] = None
        result = TelemetryValidator(30, 60).validate(json.dumps(data))
        self.assertFalse(result["time_synced"])

    def test_invalid_imu_report_with_null_measurements_is_valid_telemetry(self) -> None:
        data = telemetry(valid=False)
        result = TelemetryValidator(30, 60).validate(json.dumps(data), time.time())
        self.assertFalse(result["level"]["valid"])
        self.assertIsNone(result["level"]["pitch_deg"])

    def test_null_measurement_is_rejected_when_level_claims_valid(self) -> None:
        data = telemetry()
        data["level"]["pitch_deg"] = None
        with self.assertRaisesRegex(MessageValidationError, "pitch"):
            TelemetryValidator(30, 60).validate(json.dumps(data), time.time())

    def test_status_and_sequence_types_are_strict(self) -> None:
        data = telemetry()
        data["imu"]["healthy"] = "yes"
        with self.assertRaisesRegex(MessageValidationError, "imu.healthy"):
            TelemetryValidator(30, 60).validate(json.dumps(data), time.time())
        data = telemetry()
        data["sequence"] = 1.5
        with self.assertRaisesRegex(MessageValidationError, "integers"):
            TelemetryValidator(30, 60).validate(json.dumps(data), time.time())

    def test_dimensions_and_normalization_are_checked(self) -> None:
        data = telemetry()
        data["level"]["dimensions_mm"]["front_track"] = 100
        with self.assertRaises(MessageValidationError):
            TelemetryValidator(30, 60).validate(json.dumps(data), time.time())
        data = telemetry()
        data["level"]["wheel_mm"] = {"fl": 2, "fr": 3, "rl": 4, "rr": 5}
        with self.assertRaisesRegex(MessageValidationError, "normalized"):
            TelemetryValidator(30, 60).validate(json.dumps(data), time.time())


class FakeResult:
    rc = 0


class FakeClient:
    def __init__(self) -> None:
        self.calls: list[tuple[str, str, int, bool]] = []

    def publish(self, topic: str, payload: str, qos: int, retain: bool) -> FakeResult:
        self.calls.append((topic, payload, qos, retain))
        return FakeResult()


class ReceiverTests(unittest.TestCase):
    def test_settings_persist_and_use_non_retained_qos1(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            filename = Path(folder) / "settings.json"
            receiver = MqttReceiver(MqttConfig("broker", 1883, "", "", "camper/level", "test", 30),
                                    30, 60, level_settings_file=filename)
            client = FakeClient()
            receiver._client, receiver._transport_connected = client, True
            self.assertTrue(receiver.publish_level_setting("wheelbase_mm", 3350))
            self.assertEqual(json.loads(filename.read_text()), {"wheelbase_mm": 3350})
            self.assertEqual(client.calls[0],
                             ("camper/level/config/set/wheelbase_mm", "3350", 1, False))
            self.assertFalse(receiver.publish_level_setting("wheelbase_mm", 100))

    def test_level_zero_request_and_ack(self) -> None:
        receiver = MqttReceiver(MqttConfig("broker", 1883, "", "", "camper/level", "test", 30), 30, 60)
        client = FakeClient()
        receiver._client, receiver._transport_connected = client, True
        self.assertTrue(receiver.request_level_zero(27))

        class Message:
            topic = "camper/level/config/ack"
            payload = b'{"key":"level_zero","accepted":true,"value":"27"}'

        receiver._on_message(None, None, Message())
        acknowledgement = receiver.take_level_zero_ack()
        self.assertEqual(acknowledgement.request_id, 27)
        self.assertTrue(acknowledgement.accepted)


class FakeVeDbusService:
    def __init__(self, name: str, bus: object = None, register: bool = False) -> None:
        self.name, self.bus, self.registered = name, bus, register
        self.values: dict[str, object] = {}
        self.callbacks: dict[str, object] = {}

    def add_path(self, path: str, value: object = None, **kwargs: object) -> None:
        self.values[path] = value
        if kwargs.get("onchangecallback") is not None:
            self.callbacks[path] = kwargs["onchangecallback"]

    def register(self) -> None:
        self.registered = True

    def __getitem__(self, path: str) -> object:
        return self.values[path]

    def __setitem__(self, path: str, value: object) -> None:
        self.values[path] = value


class DbusTests(unittest.TestCase):
    def config(self) -> AppConfig:
        return AppConfig(
            MqttConfig("broker", 1883, "", "", "camper/level", "test", 30),
            ServiceConfig(
                dbus_name="com.victronenergy.camperlevel", device_instance=42,
                native_dbus_name="com.victronenergy.switch.camperlevel",
                native_device_instance=42, product_name="Camper Level",
                custom_name="Camper Level", serial="test", firmware_version="1.0.0",
                warning_timeout_s=10, critical_timeout_s=30,
                message_max_age_s=30, future_tolerance_s=60,
                level_settings_file="settings.json", log_level="INFO"),
        )

    def create(self) -> DbusCamperLevelService:
        with (patch.object(dbus_module, "_load_vedbus_service", return_value=FakeVeDbusService),
              patch.object(dbus_module, "_new_dbus_connection", return_value=object())):
            return DbusCamperLevelService(self.config(),
                                          level_setting_writer=lambda _key, _value: True,
                                          level_zero_writer=lambda _request: True)

    def test_valid_snapshot_maps_and_offline_clears_live_values(self) -> None:
        service = self.create()
        stats = ReceiverStats(1, 0, 0, 0, 0)
        service.apply_snapshot(Snapshot(telemetry(), 10.0, 1000.0), stats)
        self.assertEqual(service._service["/Connected"], 1)
        self.assertEqual(service._service["/Level/Wheels/FrontLeft"], 12.0)
        self.assertEqual(service._native_service["/ProductId"], 0xC512)
        service.apply_communication_state(30.0, 2, stats)
        self.assertEqual(service._service["/Connected"], 0)
        self.assertIsNone(service._service["/Level/Pitch"])
        self.assertEqual(service._native_service["/GenericInput/8/Value"], 0)

    def test_sensor_fault_stays_connected_and_is_not_reported_as_offline(self) -> None:
        service = self.create()
        stats = ReceiverStats(1, 0, 0, 0, 0)
        service.apply_snapshot(Snapshot(telemetry(valid=False), 10.0, 1000.0), stats)
        service.apply_communication_state(0.1, 0, stats)
        self.assertEqual(service._service["/Connected"], 1)
        self.assertEqual(service._service["/Alarms/IMU"], 2)
        self.assertEqual(service._service["/Level/State"], "FAULT")
        self.assertIsNone(service._service["/Level/Pitch"])
        self.assertEqual(service._native_service["/GenericInput/8/Value"], 1)
        self.assertEqual(service._native_service["/GenericInput/13/Value"], 1)

    def test_zero_ack_and_settings_callbacks(self) -> None:
        service = self.create()
        callback = service._native_service.callbacks["/GenericInput/9/Value"]
        self.assertTrue(callback("/GenericInput/9/Value", 3500))
        self.assertFalse(callback("/GenericInput/9/Value", 100))
        service.apply_level_zero_ack(LevelZeroAck(7, True, 1000))
        self.assertEqual(service._native_service["/Settings/Level/ZeroStatus"], 2)


class CommunicationTests(unittest.TestCase):
    def test_thresholds(self) -> None:
        self.assertEqual(communication_severity(1, True, True, 10, 30), 0)
        self.assertEqual(communication_severity(10, True, True, 10, 30), 1)
        self.assertEqual(communication_severity(30, True, True, 10, 30), 2)
        self.assertEqual(communication_severity(1, False, True, 10, 30), 1)
        self.assertEqual(communication_severity(1, True, False, 10, 30), 2)
        self.assertEqual(communication_severity(None, True, None, 10, 30), 2)


if __name__ == "__main__":
    unittest.main()
