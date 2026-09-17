#!/usr/bin/env python3
"""Standalone MQTT-to-D-Bus daemon for Camper Level."""

from __future__ import annotations

import argparse
import logging
import signal
from typing import Any

from config import AppConfig
from dbus_service import DbusCamperLevelService
from mqtt_receiver import MqttReceiver


def communication_severity(age: float | None, transport: bool,
                           availability: bool | None,
                           warning_s: float, critical_s: float) -> int:
    if availability is False or age is None or age >= critical_s:
        return 2
    if not transport or age >= warning_s:
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default="/data/conf/camper-level/config.ini")
    parser.add_argument("--check-config", action="store_true",
                        help="validate configuration and exit without opening D-Bus")
    args = parser.parse_args()
    config = AppConfig.load(args.config)
    if args.check_config:
        print(f"Configuration OK: {args.config}")
        return 0
    logging.basicConfig(
        level=getattr(logging, config.service.log_level, logging.INFO),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )

    from dbus.mainloop.glib import DBusGMainLoop
    from gi.repository import GLib

    DBusGMainLoop(set_as_default=True)
    receiver = MqttReceiver(
        config.mqtt,
        config.service.message_max_age_s,
        config.service.future_tolerance_s,
        level_settings_file=config.service.level_settings_file,
    )
    service = DbusCamperLevelService(
        config,
        level_setting_writer=receiver.publish_level_setting,
        level_zero_writer=receiver.request_level_zero,
        initial_level_settings=receiver.desired_level_settings(),
    )
    receiver.start()
    loop = GLib.MainLoop()

    def tick() -> bool:
        snapshot = receiver.take_latest()
        if snapshot is not None:
            service.apply_snapshot(snapshot, receiver.stats(),
                                   receiver.desired_level_settings())
        acknowledgement = receiver.take_level_zero_ack()
        if acknowledgement is not None:
            service.apply_level_zero_ack(acknowledgement)
        age = receiver.seconds_since_valid()
        transport, availability = receiver.connection_state()
        severity = communication_severity(
            age, transport, availability,
            config.service.warning_timeout_s,
            config.service.critical_timeout_s,
        )
        service.apply_communication_state(age, severity, receiver.stats())
        return True

    GLib.timeout_add(250, tick)

    def stop(_signum: int, _frame: Any) -> None:
        loop.quit()

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        loop.run()
    finally:
        receiver.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
