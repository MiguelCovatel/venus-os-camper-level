#pragma once

#include <Arduino.h>
#include "config_manager.h"

namespace camper {
enum class ConsoleAction : uint8_t { NONE, REBOOT };

class SerialConsole {
 public:
  explicit SerialConsole(ConfigManager& config) : config_(config) {}
  ConsoleAction poll(Stream& serial, float orientedPitchDeg, float orientedRollDeg);
  static void printHelp(Stream& output);
 private:
  ConsoleAction execute(String command, Stream& output, float pitch, float roll);
  ConfigManager& config_;
  String buffer_;
};
}  // namespace camper
