#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include "communication/mqtt_transport.h"
#include "config/config_manager.h"
#include "config/serial_console.h"
#include "diagnostics/watchdog.h"
#include "level/level_calculator.h"
#include "level/stability_detector.h"
#include "sensors/imu.h"

#ifndef CAMPER_I2C_SDA
#define CAMPER_I2C_SDA 8
#endif
#ifndef CAMPER_I2C_SCL
#define CAMPER_I2C_SCL 9
#endif

namespace {
constexpr char kFirmwareVersion[] = "1.0.0";
constexpr size_t kTelemetryBytes = 1450;
camper::ConfigManager config;
camper::SerialConsole console(config);
camper::Imu imu;
camper::MqttTransport mqtt;
camper::StabilityDetector stability;
camper::LevelResult levelResult;
camper::StabilityState stabilityState = camper::StabilityState::MOVING;
camper::LevelQuality levelQuality = camper::LevelQuality::UNLEVEL;
float orientedPitch = 0, orientedRoll = 0, levelPitch = 0, levelRoll = 0;
bool levelValid = false, imuStarted = false;
uint32_t nextSampleMs = 0, nextReportMs = 0, nextImuRetryMs = 0;
char telemetry[kTelemetryBytes];

void applyOrientation(float sensorPitch, float sensorRoll, float& pitch, float& roll) {
  const auto& settings = config.level();
  pitch = settings.swapAxes ? sensorRoll : sensorPitch;
  roll = settings.swapAxes ? sensorPitch : sensorRoll;
  if (settings.invertPitch) pitch = -pitch;
  if (settings.invertRoll) roll = -roll;
}

bool applyRemoteSetting(const char* key, const char* value, String& error) {
  if (!strcmp(key, "level_zero")) {
    char* end = nullptr;
    const unsigned long requestId = strtoul(value, &end, 10);
    if (end == value || *end || requestId < 1 || requestId > 2147483647UL) { error = "invalid level-zero request id"; return false; }
    if (!levelValid || !isfinite(orientedPitch) || !isfinite(orientedRoll)) { error = "IMU level reading is not valid"; return false; }
    if (!config.setLevelZero(orientedPitch, orientedRoll)) { error = "NVS write failed"; return false; }
    stability.reset(); nextSampleMs = 0; nextReportMs = 0;
    Serial.printf("INFO remote level zero saved: request=%lu pitch=%.4f roll=%.4f\n", requestId, orientedPitch, orientedRoll);
    return true;
  }
  const char* allowed[] = {"wheelbase_mm", "front_track_mm", "rear_track_mm", "track_width_mm",
      "invert_pitch", "invert_roll", "swap_axes", "perfect_tolerance_deg", "acceptable_tolerance_deg",
      "stable_variation_deg", "stable_duration_ms"};
  bool permitted = false;
  for (const char* candidate : allowed) if (!strcmp(key, candidate)) { permitted = true; break; }
  if (!permitted) { error = "remote setting is not allowed"; return false; }
  const bool saved = config.setValue(String(key), String(value), error);
  if (saved) { stability.reset(); nextSampleMs = 0; nextReportMs = 0; }
  return saved;
}

void jsonNumber(char* output, size_t capacity, float value, uint8_t decimals) {
  if (!isfinite(value)) snprintf(output, capacity, "null");
  else snprintf(output, capacity, "%.*f", static_cast<int>(decimals), value);
}
bool append(char* output, size_t capacity, size_t& used, const char* format, ...) {
  if (used >= capacity) return false;
  va_list args; va_start(args, format);
  const int written = vsnprintf(output + used, capacity - used, format, args);
  va_end(args);
  if (written < 0 || static_cast<size_t>(written) >= capacity - used) { output[capacity - 1] = '\0'; used = capacity; return false; }
  used += static_cast<size_t>(written); return true;
}

bool buildTelemetry(char* output, size_t capacity, uint32_t now, uint32_t sequence) {
  const bool healthy = imuStarted && imu.healthy(now);
  const char* status = healthy && levelValid ? "OK" : "FAULT";
  const uint64_t epoch = mqtt.epochMillis();
  char timestamp[24] = "null";
  if (epoch) snprintf(timestamp, sizeof(timestamp), "%llu", static_cast<unsigned long long>(epoch));
  char pitch[24], roll[24], fl[24], fr[24], rl[24], rr[24], rfl[24], rfr[24], rrl[24], rrr[24];
  jsonNumber(pitch, sizeof(pitch), levelValid ? levelPitch : NAN, 3);
  jsonNumber(roll, sizeof(roll), levelValid ? levelRoll : NAN, 3);
  jsonNumber(fl, sizeof(fl), levelValid ? levelResult.correctionMm.frontLeft : NAN, 1);
  jsonNumber(fr, sizeof(fr), levelValid ? levelResult.correctionMm.frontRight : NAN, 1);
  jsonNumber(rl, sizeof(rl), levelValid ? levelResult.correctionMm.rearLeft : NAN, 1);
  jsonNumber(rr, sizeof(rr), levelValid ? levelResult.correctionMm.rearRight : NAN, 1);
  jsonNumber(rfl, sizeof(rfl), levelValid ? levelResult.relativeHeightMm.frontLeft : NAN, 1);
  jsonNumber(rfr, sizeof(rfr), levelValid ? levelResult.relativeHeightMm.frontRight : NAN, 1);
  jsonNumber(rrl, sizeof(rrl), levelValid ? levelResult.relativeHeightMm.rearLeft : NAN, 1);
  jsonNumber(rrr, sizeof(rrr), levelValid ? levelResult.relativeHeightMm.rearRight : NAN, 1);
  size_t used = 0;
  bool ok = append(output, capacity, used,
      "{\"schema_version\":1,\"firmware_version\":\"%s\",\"timestamp_ms\":%s,\"time_synced\":%s,"
      "\"sample_time_ms\":%lu,\"uptime_ms\":%lu,\"sequence\":%lu,\"boot_id\":\"%s\","
      "\"device_name\":\"%s\",\"sensor_status\":\"%s\",",
      kFirmwareVersion, timestamp, epoch ? "true" : "false", static_cast<unsigned long>(now), static_cast<unsigned long>(now),
      static_cast<unsigned long>(sequence), mqtt.bootId(), config.network().deviceName, status);
  ok &= append(output, capacity, used,
      "\"imu\":{\"type\":\"%s\",\"address\":%u,\"who_am_i\":%u,\"healthy\":%s,\"fault\":\"%s\"},",
      camper::Imu::typeName(imu.type()), imu.address(), imu.whoAmI(), healthy ? "true" : "false", healthy ? "none" : imu.faultReason());
  ok &= append(output, capacity, used,
      "\"level\":{\"valid\":%s,\"pitch_deg\":%s,\"roll_deg\":%s,\"state\":\"%s\",\"stable\":%s,\"quality\":\"%s\","
      "\"dimensions_mm\":{\"wheelbase\":%.1f,\"front_track\":%.1f,\"rear_track\":%.1f},"
      "\"wheel_mm\":{\"fl\":%s,\"fr\":%s,\"rl\":%s,\"rr\":%s},"
      "\"relative_height_mm\":{\"fl\":%s,\"fr\":%s,\"rl\":%s,\"rr\":%s}},",
      levelValid ? "true" : "false", pitch, roll, camper::StabilityDetector::stateName(stabilityState),
      levelValid && stabilityState == camper::StabilityState::STABLE ? "true" : "false",
      camper::LevelCalculator::qualityName(levelQuality), config.level().wheelbaseMm, config.level().frontTrackMm,
      config.level().rearTrackMm, fl, fr, rl, rr, rfl, rfr, rrl, rrr);
  const auto& network = mqtt.diagnostics();
  ok &= append(output, capacity, used,
      "\"network\":{\"state\":\"%s\",\"rssi_dbm\":%d,\"mqtt_error\":%d,\"last_publish_ms\":%lu,\"publish_failures\":%lu},"
      "\"diagnostics\":{\"reset_reason\":\"%s\"}}",
      camper::MqttTransport::stateName(network.state), static_cast<int>(network.rssiDbm), network.mqttError,
      static_cast<unsigned long>(network.lastPublishMs), static_cast<unsigned long>(network.publishFailures), camper::Watchdog::resetReason());
  return ok;
}

void publishTelemetry(uint32_t now) {
  const uint32_t sequence = mqtt.beginBatch();
  const uint64_t timestamp = mqtt.epochMillis();
  const bool healthy = imuStarted && imu.healthy(now);
  const char* status = healthy && levelValid ? "OK" : "FAULT";
  bool ok = buildTelemetry(telemetry, sizeof(telemetry), now, sequence) && mqtt.publishRaw("data", telemetry, false);
  auto number = [&](const char* topic, float value, uint8_t decimals) {
    char text[24]; jsonNumber(text, sizeof(text), value, decimals);
    ok &= mqtt.publishEnvelope(topic, text, timestamp, now, sequence, status);
  };
  auto text = [&](const char* topic, const char* value) {
    char quoted[64]; const int n = snprintf(quoted, sizeof(quoted), "\"%s\"", value);
    ok &= n > 0 && static_cast<size_t>(n) < sizeof(quoted) && mqtt.publishEnvelope(topic, quoted, timestamp, now, sequence, status);
  };
  auto integer = [&](const char* topic, uint32_t value) {
    char valueText[16];
    snprintf(valueText, sizeof(valueText), "%lu", static_cast<unsigned long>(value));
    ok &= mqtt.publishEnvelope(topic, valueText, timestamp, now, sequence, status);
  };
  text("status", levelValid ? "ONLINE" : "SENSOR_FAULT");
  number("pitch", levelValid ? levelPitch : NAN, 3); number("roll", levelValid ? levelRoll : NAN, 3);
  ok &= mqtt.publishEnvelope("stable", levelValid && stabilityState == camper::StabilityState::STABLE ? "true" : "false", timestamp, now, sequence, status);
  text("state", camper::StabilityDetector::stateName(stabilityState));
  text("quality", camper::LevelCalculator::qualityName(levelQuality));
  number("wheels/fl", levelValid ? levelResult.correctionMm.frontLeft : NAN, 1);
  number("wheels/fr", levelValid ? levelResult.correctionMm.frontRight : NAN, 1);
  number("wheels/rl", levelValid ? levelResult.correctionMm.rearLeft : NAN, 1);
  number("wheels/rr", levelValid ? levelResult.correctionMm.rearRight : NAN, 1);
  integer("diagnostic/uptime", now);
  number("diagnostic/rssi", static_cast<float>(mqtt.diagnostics().rssiDbm), 0);
  mqtt.finishBatch(now, ok);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialDeadline = millis() + 1500;
  while (!Serial && millis() < serialDeadline) delay(10);
  Serial.printf("Camper Level Monitor %s - ESP32-C3 + MPU only; reset=%s\n", kFirmwareVersion, camper::Watchdog::resetReason());
  if (!config.begin()) Serial.println("ERROR NVS configuration initialization failed");
  config.print(Serial); camper::SerialConsole::printHelp(Serial);
  imuStarted = imu.begin(Wire, CAMPER_I2C_SDA, CAMPER_I2C_SCL);
  if (imuStarted) Serial.printf("IMU detected: %s at 0x%02X (WHO_AM_I=0x%02X)\n", camper::Imu::typeName(imu.type()), imu.address(), imu.whoAmI());
  else { Serial.printf("CRITICAL IMU unavailable: %s\n", imu.faultReason()); nextImuRetryMs = millis() + 10000; }
  if (!camper::Watchdog::begin()) Serial.println("ERROR task watchdog initialization failed");
  mqtt.setConfigCommandHandler(applyRemoteSetting);
  mqtt.begin(config.network(), millis());
}

void loop() {
  const uint32_t now = millis();
  camper::Watchdog::feed();
  mqtt.update(config.network(), now);
  if (!imuStarted && static_cast<int32_t>(now - nextImuRetryMs) >= 0) {
    nextImuRetryMs = now + 10000;
    imuStarted = imu.begin(Wire, CAMPER_I2C_SDA, CAMPER_I2C_SCL);
    if (imuStarted) Serial.printf("INFO IMU recovered: %s at 0x%02X\n", camper::Imu::typeName(imu.type()), imu.address());
  }
  if (imuStarted && static_cast<int32_t>(now - nextSampleMs) >= 0) {
    nextSampleMs = now + 20;
    if (imu.update(micros(), now)) {
      applyOrientation(imu.attitude().pitchDeg, imu.attitude().rollDeg, orientedPitch, orientedRoll);
      const auto& settings = config.level();
      levelPitch = orientedPitch - settings.pitchOffsetDeg;
      levelRoll = orientedRoll - settings.rollOffsetDeg;
      stabilityState = stability.update(levelPitch, levelRoll, now, settings.stableVariationDeg, settings.stableDurationMs);
      levelResult = camper::LevelCalculator::calculate(levelPitch, levelRoll, settings.wheelbaseMm, settings.frontTrackMm, settings.rearTrackMm);
      levelQuality = camper::LevelCalculator::quality(levelPitch, levelRoll, settings.perfectToleranceDeg, settings.acceptableToleranceDeg);
      levelValid = true;
    } else if (!imu.healthy(now)) {
      levelValid = false; stabilityState = camper::StabilityState::MOVING;
    }
  }
  if (static_cast<int32_t>(now - nextReportMs) >= 0) {
    nextReportMs = now + 500;
    if (buildTelemetry(telemetry, sizeof(telemetry), now, 0)) Serial.println(telemetry);
    else Serial.println("ERROR telemetry JSON buffer exhausted");
  }
  if (mqtt.shouldPublish(now)) publishTelemetry(now);
  if (console.poll(Serial, levelValid ? orientedPitch : NAN, levelValid ? orientedRoll : NAN) == camper::ConsoleAction::REBOOT) {
    Serial.println("INFO rebooting"); Serial.flush(); delay(100); ESP.restart();
  }
  delay(1);
}
