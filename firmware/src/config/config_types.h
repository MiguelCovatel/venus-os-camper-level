#pragma once

#include <stdint.h>

namespace camper {

struct LevelConfig {
  float wheelbaseMm = 3200.0f;
  float frontTrackMm = 1800.0f;
  float rearTrackMm = 1800.0f;
  float pitchOffsetDeg = 0.0f;
  float rollOffsetDeg = 0.0f;
  bool invertPitch = false;
  bool invertRoll = false;
  bool swapAxes = false;
  // Practical "camper bubble" defaults. The IMU remains precise internally,
  // but normal suspension/floor movement is not presented as a levelling error.
  float perfectToleranceDeg = 0.5f;
  float acceptableToleranceDeg = 1.0f;
  float stableVariationDeg = 0.2f;
  uint32_t stableDurationMs = 3000;
};

struct NetworkConfig {
  char wifiSsid[33] = "";
  char wifiPassword[65] = "";
  char mqttServer[65] = "";
  uint16_t mqttPort = 1883;
  char mqttUsername[65] = "";
  char mqttPassword[65] = "";
  char mqttBaseTopic[65] = "camper/level";
  char deviceName[33] = "camper-level";
  char webPassword[65] = "camperlevel";
  uint32_t heartbeatIntervalMs = 1000;
  uint32_t reconnectMinMs = 5000;
  uint32_t reconnectMaxMs = 60000;
};

}  // namespace camper
