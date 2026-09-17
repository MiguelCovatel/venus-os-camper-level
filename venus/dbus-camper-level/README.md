# dbus-camper-level

Level-only MQTT receiver for Victron Venus OS. It publishes both a private
diagnostic service (`com.victronenergy.camperlevel`) and a native switch
service (`com.victronenergy.switch.camperlevel`, instance 42) so the device is
visible without patching Venus OS core files.

Live data is read from `camper/level/data`. Availability is read from
`camper/level/availability`; configuration is sent to
`camper/level/config/set/*` and acknowledgements arrive at
`camper/level/config/ack`.

The installer preserves `/data/conf/camper-level`. Release bundles should
include either a `vendor/` directory containing `paho-mqtt` or an offline wheel
under `wheels/`.
