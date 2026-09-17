#include "serial_console.h"

namespace camper {

void SerialConsole::printHelp(Stream& output) {
  output.println("Commands: SHOW, ZERO, SET <key> <value>, HELP, REBOOT");
  output.println("          FACTORY_RESET CONFIRM (clears only camperlevel NVS)");
  output.println("Level keys: wheelbase_mm, front_track_mm, rear_track_mm, track_width_mm,");
  output.println("  invert_pitch, invert_roll, swap_axes, perfect_tolerance_deg,");
  output.println("  acceptable_tolerance_deg, stable_variation_deg, stable_duration_ms");
  output.println("Network keys: wifi_ssid, wifi_password, mqtt_server, mqtt_port,");
  output.println("  mqtt_username, mqtt_password, mqtt_base_topic, device_name,");
  output.println("  heartbeat_interval_ms, reconnect_min_ms, reconnect_max_ms");
  output.println("Use '-' as an empty text value. Example: SET mqtt_username -");
}

ConsoleAction SerialConsole::poll(Stream& serial, float pitch, float roll) {
  while (serial.available()) {
    const char input = static_cast<char>(serial.read());
    if (input == '\r') continue;
    if (input == '\n') {
      const ConsoleAction result = execute(buffer_, serial, pitch, roll);
      buffer_ = "";
      return result;
    }
    if (buffer_.length() < 160) buffer_ += input;
  }
  return ConsoleAction::NONE;
}

ConsoleAction SerialConsole::execute(String command, Stream& output, float pitch, float roll) {
  command.trim();
  if (!command.length()) return ConsoleAction::NONE;
  if (command.equalsIgnoreCase("HELP")) { printHelp(output); return ConsoleAction::NONE; }
  if (command.equalsIgnoreCase("SHOW")) { config_.print(output); return ConsoleAction::NONE; }
  if (command.equalsIgnoreCase("ZERO")) {
    output.println(config_.setLevelZero(pitch, roll) ? "OK level zero saved" : "ERROR valid IMU reading required or NVS failed");
    return ConsoleAction::NONE;
  }
  if (command.equalsIgnoreCase("REBOOT")) return ConsoleAction::REBOOT;
  if (command.equalsIgnoreCase("FACTORY_RESET CONFIRM")) {
    output.println(config_.factoryReset() ? "OK standalone configuration reset; use REBOOT" : "ERROR NVS reset failed");
    return ConsoleAction::NONE;
  }
  if (command.startsWith("SET ")) {
    command.remove(0, 4);
    const int separator = command.indexOf(' ');
    if (separator <= 0) { output.println("ERROR syntax: SET <key> <value>"); return ConsoleAction::NONE; }
    const String key = command.substring(0, separator);
    String value = command.substring(separator + 1);
    value.trim();
    String error;
    if (config_.setValue(key, value, error)) output.println("OK saved");
    else output.printf("ERROR %s\n", error.c_str());
    return ConsoleAction::NONE;
  }
  output.println("ERROR unknown command; type HELP");
  return ConsoleAction::NONE;
}

}  // namespace camper
