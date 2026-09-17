#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "config_types.h"

namespace camper {

class ConfigManager {
 public:
  bool begin();
  const LevelConfig& level() const { return level_; }
  const NetworkConfig& network() const { return network_; }
  bool setValue(const String& key, const String& value, String& error);
  bool replace(const LevelConfig& level, const NetworkConfig& network, String& error);
  bool setLevelZero(float orientedPitchDeg, float orientedRollDeg);
  bool factoryReset();
  void print(Stream& output) const;

 private:
  bool validate(const LevelConfig& candidate, String& error) const;
  bool validate(const NetworkConfig& candidate, String& error) const;
  bool save();

  Preferences preferences_;
  LevelConfig level_;
  NetworkConfig network_;
};

}  // namespace camper
