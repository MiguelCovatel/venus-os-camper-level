"""MQTT transport, strict telemetry validation and persistent settings."""

from __future__ import annotations

import json
import logging
import math
import os
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

from config import MqttConfig

LOG = logging.getLogger(__name__)


class MessageValidationError(ValueError):
    """Raised when telemetry must not reach D-Bus."""


@dataclass(frozen=True)
class Snapshot:
    payload: dict[str, Any]
    received_monotonic: float
    received_wall_time: float


@dataclass(frozen=True)
class ReceiverStats:
    valid_messages: int
    invalid_messages: int
    out_of_order_messages: int
    sequence_gaps: int
    reconnects: int


@dataclass(frozen=True)
class LevelZeroAck:
    request_id: int
    accepted: bool
    received_wall_time: float


class TelemetryValidator:
    REQUIRED_TOP_LEVEL = {
        "schema_version", "timestamp_ms", "time_synced", "uptime_ms",
        "sequence", "boot_id", "device_name", "sensor_status", "imu",
        "level", "network",
    }
    WHEELS = ("fl", "fr", "rl", "rr")

    def __init__(self, message_max_age_s: float, future_tolerance_s: float) -> None:
        self.message_max_age_s = message_max_age_s
        self.future_tolerance_s = future_tolerance_s
        self.current_boot_id: str | None = None
        self.last_sequence: int | None = None
        self.sequence_gaps = 0

    @staticmethod
    def _finite(value: Any, name: str, minimum: float, maximum: float) -> float:
        if isinstance(value, bool):
            raise MessageValidationError(f"{name} must be numeric")
        try:
            numeric = float(value)
        except (TypeError, ValueError, OverflowError) as exc:
            raise MessageValidationError(f"{name} must be numeric") from exc
        if not math.isfinite(numeric) or not minimum <= numeric <= maximum:
            raise MessageValidationError(f"{name} is outside {minimum}..{maximum}")
        return numeric

    def validate(self, payload: bytes | str, now_wall_time: float | None = None) -> dict[str, Any]:
        try:
            document = json.loads(payload)
        except (TypeError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise MessageValidationError(f"invalid JSON: {exc}") from exc
        if not isinstance(document, dict):
            raise MessageValidationError("top-level JSON value must be an object")
        missing = sorted(self.REQUIRED_TOP_LEVEL.difference(document))
        if missing:
            raise MessageValidationError(f"missing fields: {', '.join(missing)}")
        if document["schema_version"] != 1:
            raise MessageValidationError("unsupported schema_version")
        for field in ("imu", "level", "network"):
            if not isinstance(document[field], dict):
                raise MessageValidationError(f"{field} must be an object")
        if not isinstance(document["time_synced"], bool):
            raise MessageValidationError("time_synced must be a boolean")
        if not isinstance(document["imu"].get("healthy"), bool):
            raise MessageValidationError("imu.healthy must be a boolean")
        if not isinstance(document["device_name"], str) or not document["device_name"].strip():
            raise MessageValidationError("device_name must be a non-empty string")
        if not isinstance(document["sensor_status"], str) or not document["sensor_status"].strip():
            raise MessageValidationError("sensor_status must be a non-empty string")

        level = document["level"]
        wheels = level.get("wheel_mm")
        if not isinstance(wheels, dict) or not all(key in wheels for key in self.WHEELS):
            raise MessageValidationError("level.wheel_mm is incomplete")
        if not isinstance(level.get("valid"), bool) or not isinstance(level.get("stable"), bool):
            raise MessageValidationError("level.valid and level.stable must be booleans")
        if level["valid"]:
            values = [self._finite(wheels[key], f"wheel_mm.{key}", -0.5, 5000) for key in self.WHEELS]
            if min(values) > 1.0:
                raise MessageValidationError("wheel corrections are not normalized")
            self._finite(level.get("pitch_deg"), "level.pitch_deg", -89.0, 89.0)
            self._finite(level.get("roll_deg"), "level.roll_deg", -89.0, 89.0)
            if str(level.get("state", "")).upper() not in {"MOVING", "STABILIZING", "STABLE"}:
                raise MessageValidationError("unknown level.state")
            if str(level.get("quality", "")).upper() not in {
                "LEVEL", "PERFECT", "ACCEPTABLE", "SLIGHTLY_UNLEVEL", "UNLEVEL"
            }:
                raise MessageValidationError("unknown level.quality")
        else:
            for name, value in (("pitch_deg", level.get("pitch_deg")),
                                ("roll_deg", level.get("roll_deg"))):
                if value is not None:
                    self._finite(value, f"level.{name}", -89.0, 89.0)
            for key in self.WHEELS:
                if wheels[key] is not None:
                    self._finite(wheels[key], f"wheel_mm.{key}", -0.5, 5000)

        dimensions = level.get("dimensions_mm")
        if dimensions is not None:
            if not isinstance(dimensions, dict):
                raise MessageValidationError("level.dimensions_mm must be an object")
            for key, limits in {
                "wheelbase": (500, 12000), "front_track": (500, 4000),
                "rear_track": (500, 4000),
            }.items():
                if key not in dimensions:
                    raise MessageValidationError("level.dimensions_mm is incomplete")
                self._finite(dimensions[key], f"dimensions_mm.{key}", *limits)

        if (isinstance(document["sequence"], bool)
                or not isinstance(document["sequence"], int)
                or isinstance(document["uptime_ms"], bool)
                or not isinstance(document["uptime_ms"], int)):
            raise MessageValidationError("sequence and uptime_ms must be integers")
        sequence, uptime_ms = document["sequence"], document["uptime_ms"]
        if sequence < 1 or uptime_ms < 0:
            raise MessageValidationError("invalid sequence or uptime_ms")
        boot_id = str(document["boot_id"]).strip()
        if not boot_id or len(boot_id) > 64:
            raise MessageValidationError("boot_id is empty or too long")

        if bool(document["time_synced"]):
            if (isinstance(document["timestamp_ms"], bool)
                    or not isinstance(document["timestamp_ms"], int)):
                raise MessageValidationError("timestamp_ms must be an integer")
            timestamp_s = document["timestamp_ms"] / 1000.0
            now = time.time() if now_wall_time is None else now_wall_time
            if timestamp_s > now + self.future_tolerance_s:
                raise MessageValidationError("message timestamp is too far in the future")
            if timestamp_s < now - self.message_max_age_s:
                raise MessageValidationError("message timestamp is stale")
        elif document["timestamp_ms"] is not None:
            raise MessageValidationError("unsynchronized timestamp_ms must be null")

        if boot_id == self.current_boot_id and self.last_sequence is not None:
            if sequence <= self.last_sequence:
                raise MessageValidationError("duplicate or out-of-order sequence")
            if sequence > self.last_sequence + 1:
                self.sequence_gaps += sequence - self.last_sequence - 1
        else:
            self.current_boot_id = boot_id
        self.last_sequence = sequence
        return document


class MqttReceiver:
    LEVEL_SETTING_LIMITS = {
        "wheelbase_mm": (500, 12000),
        "front_track_mm": (500, 4000),
        "rear_track_mm": (500, 4000),
    }

    def __init__(self, config: MqttConfig, message_max_age_s: float,
                 future_tolerance_s: float,
                 monotonic: Callable[[], float] = time.monotonic,
                 wall_time: Callable[[], float] = time.time,
                 level_settings_file: str | Path | None = None) -> None:
        self.config = config
        self.monotonic = monotonic
        self.wall_time = wall_time
        self.validator = TelemetryValidator(message_max_age_s, future_tolerance_s)
        self._lock = threading.Lock()
        self._latest: Snapshot | None = None
        self._latest_generation = 0
        self._consumed_generation = 0
        self._last_valid_monotonic: float | None = None
        self._availability_online: bool | None = None
        self._transport_connected = False
        self._ever_connected = False
        self._client: Any = None
        self._settings_file = Path(level_settings_file) if level_settings_file else None
        self._desired_settings = self._load_settings()
        self._pending_settings = dict(self._desired_settings)
        self._latest_zero_ack: LevelZeroAck | None = None
        self._zero_ack_generation = 0
        self._consumed_zero_ack_generation = 0
        self._valid_messages = 0
        self._invalid_messages = 0
        self._out_of_order_messages = 0
        self._reconnects = 0

    @property
    def data_topic(self) -> str:
        return f"{self.config.base_topic}/data"

    @property
    def availability_topic(self) -> str:
        return f"{self.config.base_topic}/availability"

    @property
    def config_ack_topic(self) -> str:
        return f"{self.config.base_topic}/config/ack"

    @classmethod
    def _normalise_setting(cls, key: str, value: Any) -> int | None:
        if key not in cls.LEVEL_SETTING_LIMITS or isinstance(value, bool):
            return None
        try:
            numeric = int(round(float(value)))
        except (TypeError, ValueError, OverflowError):
            return None
        lower, upper = cls.LEVEL_SETTING_LIMITS[key]
        return numeric if lower <= numeric <= upper else None

    def _load_settings(self) -> dict[str, int]:
        if self._settings_file is None or not self._settings_file.exists():
            return {}
        try:
            document = json.loads(self._settings_file.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            LOG.error("Cannot load level settings: %s", exc)
            return {}
        if not isinstance(document, dict):
            return {}
        result: dict[str, int] = {}
        for key, value in document.items():
            numeric = self._normalise_setting(key, value)
            if numeric is not None:
                result[key] = numeric
        return result

    def _persist_settings(self, settings: dict[str, int]) -> bool:
        if self._settings_file is None:
            return True
        temporary = self._settings_file.with_suffix(self._settings_file.suffix + ".tmp")
        try:
            self._settings_file.parent.mkdir(parents=True, exist_ok=True)
            temporary.write_text(json.dumps(settings, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            os.replace(temporary, self._settings_file)
            return True
        except OSError as exc:
            LOG.error("Cannot persist level settings: %s", exc)
            return False

    def desired_level_settings(self) -> dict[str, int]:
        with self._lock:
            return dict(self._desired_settings)

    def publish_level_setting(self, key: str, value: int | float) -> bool:
        numeric = self._normalise_setting(key, value)
        if numeric is None:
            return False
        with self._lock:
            updated = dict(self._desired_settings)
        updated[key] = numeric
        if not self._persist_settings(updated):
            return False
        with self._lock:
            self._desired_settings = updated
            self._pending_settings[key] = numeric
        self.flush_pending_settings()
        return True

    def request_level_zero(self, request_id: int) -> bool:
        if isinstance(request_id, bool):
            return False
        try:
            numeric = int(request_id)
        except (TypeError, ValueError, OverflowError):
            return False
        if not 1 <= numeric <= 2_147_483_647:
            return False
        with self._lock:
            client, connected = self._client, self._transport_connected
        if client is None or not connected:
            return False
        info = client.publish(f"{self.config.base_topic}/config/set/level_zero",
                              payload=str(numeric), qos=1, retain=False)
        return int(getattr(info, "rc", -1)) == 0

    def flush_pending_settings(self) -> None:
        with self._lock:
            client, connected = self._client, self._transport_connected
            pending = dict(self._pending_settings)
        if client is None or not connected:
            return
        for key, value in pending.items():
            info = client.publish(f"{self.config.base_topic}/config/set/{key}",
                                  payload=str(value), qos=1, retain=False)
            if int(getattr(info, "rc", -1)) != 0:
                LOG.warning("MQTT could not queue setting %s", key)

    def start(self) -> None:
        import paho.mqtt.client as mqtt
        kwargs: dict[str, Any] = {"client_id": self.config.client_id, "protocol": mqtt.MQTTv311}
        if hasattr(mqtt, "CallbackAPIVersion"):
            kwargs["callback_api_version"] = mqtt.CallbackAPIVersion.VERSION2
        self._client = mqtt.Client(**kwargs)
        if self.config.username:
            self._client.username_pw_set(self.config.username, self.config.password)
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message
        self._client.reconnect_delay_set(min_delay=1, max_delay=60)
        self._client.connect_async(self.config.host, self.config.port,
                                   keepalive=self.config.keepalive_s)
        self._client.loop_start()

    def stop(self) -> None:
        client = self._client
        if client is None:
            return
        try:
            client.disconnect()
        finally:
            client.loop_stop()
            self._client = None

    def _on_connect(self, client: Any, _userdata: Any, _flags: Any,
                    reason_code: Any, *_: Any) -> None:
        code = int(getattr(reason_code, "value", reason_code))
        with self._lock:
            self._transport_connected = code == 0
            if code == 0:
                if self._ever_connected:
                    self._reconnects += 1
                self._ever_connected = True
        if code != 0:
            LOG.error("MQTT connection rejected: %s", reason_code)
            return
        client.subscribe([(self.availability_topic, 1), (self.data_topic, 0),
                          (self.config_ack_topic, 1)])
        self.flush_pending_settings()
        LOG.info("MQTT connected to %s:%d", self.config.host, self.config.port)

    def _on_disconnect(self, _client: Any, _userdata: Any, *args: Any) -> None:
        with self._lock:
            self._transport_connected = False
        reason = args[-2] if len(args) >= 2 else (args[-1] if args else "unknown")
        LOG.warning("MQTT disconnected: %s", reason)

    def _handle_ack(self, payload: bytes | str) -> None:
        try:
            document = json.loads(payload)
        except (TypeError, UnicodeDecodeError, json.JSONDecodeError):
            return
        if not isinstance(document, dict):
            return
        key, accepted = str(document.get("key", "")), bool(document.get("accepted", False))
        if key == "level_zero":
            try:
                request_id = int(document.get("value"))
            except (TypeError, ValueError, OverflowError):
                return
            with self._lock:
                self._latest_zero_ack = LevelZeroAck(request_id, accepted, self.wall_time())
                self._zero_ack_generation += 1
            return
        numeric = self._normalise_setting(key, document.get("value"))
        if accepted and numeric is not None:
            with self._lock:
                if self._desired_settings.get(key) == numeric:
                    self._pending_settings.pop(key, None)

    def _on_message(self, _client: Any, _userdata: Any, message: Any) -> None:
        if message.topic == self.availability_topic:
            state = message.payload.decode("utf-8", errors="replace").strip().lower()
            with self._lock:
                self._availability_online = state == "online"
            return
        if message.topic == self.config_ack_topic:
            self._handle_ack(message.payload)
            return
        if message.topic != self.data_topic:
            return
        if bool(getattr(message, "retain", False)):
            with self._lock:
                self._invalid_messages += 1
            LOG.warning("Ignoring retained live telemetry")
            return
        try:
            document = self.validator.validate(message.payload, self.wall_time())
        except MessageValidationError as exc:
            with self._lock:
                self._invalid_messages += 1
                if "duplicate or out-of-order" in str(exc):
                    self._out_of_order_messages += 1
            LOG.warning("Invalid telemetry: %s", exc)
            return
        now_mono, now_wall = self.monotonic(), self.wall_time()
        with self._lock:
            self._latest = Snapshot(document, now_mono, now_wall)
            self._latest_generation += 1
            self._last_valid_monotonic = now_mono
            self._valid_messages += 1

    def take_latest(self) -> Snapshot | None:
        with self._lock:
            if self._latest_generation == self._consumed_generation:
                return None
            self._consumed_generation = self._latest_generation
            return self._latest

    def take_level_zero_ack(self) -> LevelZeroAck | None:
        with self._lock:
            if self._zero_ack_generation == self._consumed_zero_ack_generation:
                return None
            self._consumed_zero_ack_generation = self._zero_ack_generation
            return self._latest_zero_ack

    def seconds_since_valid(self) -> float | None:
        with self._lock:
            last = self._last_valid_monotonic
        return None if last is None else max(0.0, self.monotonic() - last)

    def connection_state(self) -> tuple[bool, bool | None]:
        with self._lock:
            return self._transport_connected, self._availability_online

    def stats(self) -> ReceiverStats:
        with self._lock:
            return ReceiverStats(self._valid_messages, self._invalid_messages,
                                 self._out_of_order_messages,
                                 self.validator.sequence_gaps, self._reconnects)
