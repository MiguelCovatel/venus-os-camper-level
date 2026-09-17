#!/usr/bin/env python3
"""Publish level-only telemetry so the Venus UI can be developed without hardware."""

from __future__ import annotations

import argparse
import json
import math
import signal
import threading
import time


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--username", default="")
    parser.add_argument("--password", default="")
    parser.add_argument("--base-topic", default="camper/level")
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--duration", type=float, default=0.0)
    parser.add_argument("--scenario", choices=("steady", "moving", "imu-fault"), default="steady")
    args = parser.parse_args()

    import paho.mqtt.client as mqtt
    client = mqtt.Client(client_id=f"camper-level-simulator-{int(time.time())}")
    if args.username:
        client.username_pw_set(args.username, args.password)
    base_topic = args.base_topic.strip("/")
    availability = f"{base_topic}/availability"
    data_topic = f"{base_topic}/data"
    command_prefix = f"{base_topic}/config/set/"
    ack_topic = f"{base_topic}/config/ack"
    lock = threading.Lock()
    state: dict[str, float] = {
        "wheelbase": 3500.0, "front_track": 1800.0, "rear_track": 1780.0,
        "pitch_zero": 0.0, "roll_zero": 0.0, "last_pitch": 0.0, "last_roll": 0.0,
    }

    def on_connect(connected_client: object, _userdata: object, _flags: object,
                   _reason_code: object, *_extra: object) -> None:
        connected_client.subscribe(f"{command_prefix}+", qos=1)

    def on_message(_client: object, _userdata: object, message: object) -> None:
        topic = str(message.topic)
        if not topic.startswith(command_prefix):
            return
        key = topic[len(command_prefix):]
        value = message.payload.decode("utf-8", errors="replace").strip()
        accepted, error = False, "unsupported setting"
        limits = {"wheelbase_mm": (500, 12000), "front_track_mm": (500, 4000),
                  "rear_track_mm": (500, 4000), "track_width_mm": (500, 4000)}
        if key in limits:
            try:
                numeric = int(round(float(value)))
            except ValueError:
                error = "not numeric"
            else:
                minimum, maximum = limits[key]
                if minimum <= numeric <= maximum:
                    with lock:
                        if key == "track_width_mm":
                            state["front_track"] = state["rear_track"] = float(numeric)
                        else:
                            state[{"wheelbase_mm": "wheelbase",
                                   "front_track_mm": "front_track",
                                   "rear_track_mm": "rear_track"}[key]] = float(numeric)
                    accepted, error = True, ""
                else:
                    error = "out of range"
        elif key == "level_zero":
            try:
                request_id = int(value)
            except ValueError:
                error = "invalid request id"
            else:
                if request_id > 0:
                    with lock:
                        state["pitch_zero"] = state["last_pitch"]
                        state["roll_zero"] = state["last_roll"]
                    accepted, error = True, ""
                else:
                    error = "invalid request id"
        acknowledgement = {"key": key, "accepted": accepted, "value": value,
                           "error": error, "uptime_ms": int((time.monotonic()) * 1000),
                           "boot_id": "SIMULATOR"}
        client.publish(ack_topic, json.dumps(acknowledgement, separators=(",", ":")),
                       qos=1, retain=False)

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.host, args.port, 30)
    client.loop_start()
    client.publish(availability, "online", qos=1, retain=True).wait_for_publish()
    running = True

    def stop(_signum: int, _frame: object) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    started, sequence = time.monotonic(), 0
    boot_id = f"SIM{int(time.time()):X}"
    try:
        while running and (args.duration <= 0 or time.monotonic() - started < args.duration):
            sequence += 1
            elapsed = time.monotonic() - started
            moving = args.scenario == "moving"
            raw_pitch = (1.4 if moving else 0.18) * math.sin(elapsed / 5.0)
            raw_roll = (1.1 if moving else 0.24) * math.cos(elapsed / 7.0)
            with lock:
                state["last_pitch"], state["last_roll"] = raw_pitch, raw_roll
                pitch, roll = raw_pitch - state["pitch_zero"], raw_roll - state["roll_zero"]
                wheelbase = state["wheelbase"]
                front_track, rear_track = state["front_track"], state["rear_track"]
            front_left = -math.tan(math.radians(pitch)) * wheelbase / 2 - math.tan(math.radians(roll)) * front_track / 2
            front_right = -math.tan(math.radians(pitch)) * wheelbase / 2 + math.tan(math.radians(roll)) * front_track / 2
            rear_left = math.tan(math.radians(pitch)) * wheelbase / 2 - math.tan(math.radians(roll)) * rear_track / 2
            rear_right = math.tan(math.radians(pitch)) * wheelbase / 2 + math.tan(math.radians(roll)) * rear_track / 2
            relative = {"fl": front_left, "fr": front_right, "rl": rear_left, "rr": rear_right}
            highest = max(relative.values())
            wheels = {key: round(highest - value, 1) for key, value in relative.items()}
            faulty = args.scenario == "imu-fault"
            document = {
                "schema_version": 1, "timestamp_ms": int(time.time() * 1000),
                "time_synced": True, "uptime_ms": int(elapsed * 1000),
                "sequence": sequence, "boot_id": boot_id,
                "device_name": "camper-level-simulator",
                "sensor_status": "IMU_FAULT" if faulty else "OK",
                "imu": {"type": "SIMULATED", "who_am_i": 112, "healthy": not faulty},
                "level": {
                    "valid": not faulty, "pitch_deg": round(pitch, 3),
                    "roll_deg": round(roll, 3),
                    "state": "MOVING" if moving else "STABLE", "stable": not moving,
                    "quality": "UNLEVEL" if moving else "ACCEPTABLE",
                    "dimensions_mm": {"wheelbase": wheelbase,
                                      "front_track": front_track,
                                      "rear_track": rear_track},
                    "wheel_mm": wheels,
                    "relative_height_mm": {key: round(value, 1) for key, value in relative.items()},
                },
                "network": {"state": "ONLINE", "rssi_dbm": -58},
            }
            payload = json.dumps(document, separators=(",", ":"))
            client.publish(data_topic, payload, qos=0, retain=False)
            print(payload, flush=True)
            time.sleep(max(0.1, args.interval))
    finally:
        client.publish(availability, "offline", qos=1, retain=True).wait_for_publish()
        client.disconnect()
        client.loop_stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
