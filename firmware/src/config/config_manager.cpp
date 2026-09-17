#include "config_manager.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace camper {
namespace {

constexpr uint32_t kConfigVersion = 1;

bool parseBool(const String& value, bool& result) {
  if (value == "1" || value.equalsIgnoreCase("true") || value.equalsIgnoreCase("on")) {
    result = true;
    return true;
  }
  if (value == "0" || value.equalsIgnoreCase("false") || value.equalsIgnoreCase("off")) {
    result = false;
    return true;
  }
  return false;
}

template <size_t Capacity>
bool copyText(const String& value, char (&target)[Capacity], String& error) {
  if (value.length() >= Capacity) {
    error = "text value too long";
    return false;
  }
  value.toCharArray(target, Capacity);
  return true;
}

}  // namespace

bool ConfigManager::begin() {
  // Dedicated namespace: never clears or imports Camper Safety configuration.
  if (!preferences_.begin("camperlevel", false)) return false;
  const uint32_t storedVersion = preferences_.getUInt("version", 0);
  if (storedVersion > 0) {
    level_.wheelbaseMm = preferences_.getFloat("wheelbase", level_.wheelbaseMm);
    level_.frontTrackMm = preferences_.getFloat("fronttrack", level_.frontTrackMm);
    level_.rearTrackMm = preferences_.getFloat("reartrack", level_.rearTrackMm);
    level_.pitchOffsetDeg = preferences_.getFloat("pitchoffset", level_.pitchOffsetDeg);
    level_.rollOffsetDeg = preferences_.getFloat("rolloffset", level_.rollOffsetDeg);
    level_.invertPitch = preferences_.getBool("invpitch", level_.invertPitch);
    level_.invertRoll = preferences_.getBool("invroll", level_.invertRoll);
    level_.swapAxes = preferences_.getBool("swapaxes", level_.swapAxes);
    level_.perfectToleranceDeg = preferences_.getFloat("perfecttol", level_.perfectToleranceDeg);
    level_.acceptableToleranceDeg = preferences_.getFloat("accepttol", level_.acceptableToleranceDeg);
    level_.stableVariationDeg = preferences_.getFloat("stablevar", level_.stableVariationDeg);
    level_.stableDurationMs = preferences_.getUInt("stablems", level_.stableDurationMs);
    preferences_.getString("wifissid", network_.wifiSsid, sizeof(network_.wifiSsid));
    preferences_.getString("wifipass", network_.wifiPassword, sizeof(network_.wifiPassword));
    preferences_.getString("mqtthost", network_.mqttServer, sizeof(network_.mqttServer));
    network_.mqttPort = static_cast<uint16_t>(preferences_.getUInt("mqttport", network_.mqttPort));
    preferences_.getString("mqttuser", network_.mqttUsername, sizeof(network_.mqttUsername));
    preferences_.getString("mqttpass", network_.mqttPassword, sizeof(network_.mqttPassword));
    preferences_.getString("mqtttopic", network_.mqttBaseTopic, sizeof(network_.mqttBaseTopic));
    preferences_.getString("devicename", network_.deviceName, sizeof(network_.deviceName));
    network_.heartbeatIntervalMs = preferences_.getUInt("heartms", network_.heartbeatIntervalMs);
    network_.reconnectMinMs = preferences_.getUInt("retrymin", network_.reconnectMinMs);
    network_.reconnectMaxMs = preferences_.getUInt("retrymax", network_.reconnectMaxMs);
  }

  String error;
  bool repaired = false;
  if (!validate(level_, error)) { level_ = LevelConfig{}; repaired = true; }
  if (!validate(network_, error)) { network_ = NetworkConfig{}; repaired = true; }
  return (storedVersion != kConfigVersion || repaired) ? save() : true;
}

bool ConfigManager::validate(const LevelConfig& candidate, String& error) const {
  if (!isfinite(candidate.wheelbaseMm) || candidate.wheelbaseMm < 500.0f || candidate.wheelbaseMm > 12000.0f) {
    error = "wheelbase_mm must be 500..12000";
    return false;
  }
  if (!isfinite(candidate.frontTrackMm) || candidate.frontTrackMm < 500.0f || candidate.frontTrackMm > 4000.0f ||
      !isfinite(candidate.rearTrackMm) || candidate.rearTrackMm < 500.0f || candidate.rearTrackMm > 4000.0f) {
    error = "track must be 500..4000";
    return false;
  }
  if (!isfinite(candidate.pitchOffsetDeg) || !isfinite(candidate.rollOffsetDeg) ||
      fabsf(candidate.pitchOffsetDeg) > 180.0f || fabsf(candidate.rollOffsetDeg) > 180.0f) {
    error = "invalid level offsets";
    return false;
  }
  if (!isfinite(candidate.perfectToleranceDeg) || candidate.perfectToleranceDeg < 0.01f || candidate.perfectToleranceDeg > 5.0f ||
      !isfinite(candidate.acceptableToleranceDeg) || candidate.acceptableToleranceDeg < candidate.perfectToleranceDeg || candidate.acceptableToleranceDeg > 10.0f) {
    error = "invalid level tolerances";
    return false;
  }
  if (!isfinite(candidate.stableVariationDeg) || candidate.stableVariationDeg < 0.01f || candidate.stableVariationDeg > 2.0f ||
      candidate.stableDurationMs < 500 || candidate.stableDurationMs > 60000) {
    error = "invalid stability settings";
    return false;
  }
  return true;
}

bool ConfigManager::validate(const NetworkConfig& candidate, String& error) const {
  if (candidate.mqttPort == 0) { error = "mqtt_port must be 1..65535"; return false; }
  if (candidate.mqttBaseTopic[0] == '\0' || candidate.mqttBaseTopic[0] == '/' ||
      strchr(candidate.mqttBaseTopic, '#') || strchr(candidate.mqttBaseTopic, '+')) {
    error = "mqtt_base_topic must be relative and contain no wildcards";
    return false;
  }
  const size_t topicLength = strlen(candidate.mqttBaseTopic);
  if (candidate.mqttBaseTopic[topicLength - 1] == '/') { error = "mqtt_base_topic must not end with slash"; return false; }
  if (candidate.deviceName[0] == '\0') { error = "device_name must not be empty"; return false; }
  for (const char* c = candidate.deviceName; *c; ++c) {
    if (!(isalnum(static_cast<unsigned char>(*c)) || *c == '-' || *c == '_')) {
      error = "device_name allows only letters, numbers, dash and underscore";
      return false;
    }
  }
  if (candidate.heartbeatIntervalMs < 250 || candidate.heartbeatIntervalMs > 60000 ||
      candidate.reconnectMinMs < 1000 || candidate.reconnectMinMs > candidate.reconnectMaxMs || candidate.reconnectMaxMs > 600000) {
    error = "invalid heartbeat/reconnect timing";
    return false;
  }
  return true;
}

bool ConfigManager::save() {
  bool ok = true;
  ok &= preferences_.putFloat("wheelbase", level_.wheelbaseMm) == sizeof(float);
  ok &= preferences_.putFloat("fronttrack", level_.frontTrackMm) == sizeof(float);
  ok &= preferences_.putFloat("reartrack", level_.rearTrackMm) == sizeof(float);
  ok &= preferences_.putFloat("pitchoffset", level_.pitchOffsetDeg) == sizeof(float);
  ok &= preferences_.putFloat("rolloffset", level_.rollOffsetDeg) == sizeof(float);
  ok &= preferences_.putBool("invpitch", level_.invertPitch) == sizeof(bool);
  ok &= preferences_.putBool("invroll", level_.invertRoll) == sizeof(bool);
  ok &= preferences_.putBool("swapaxes", level_.swapAxes) == sizeof(bool);
  ok &= preferences_.putFloat("perfecttol", level_.perfectToleranceDeg) == sizeof(float);
  ok &= preferences_.putFloat("accepttol", level_.acceptableToleranceDeg) == sizeof(float);
  ok &= preferences_.putFloat("stablevar", level_.stableVariationDeg) == sizeof(float);
  ok &= preferences_.putUInt("stablems", level_.stableDurationMs) == sizeof(uint32_t);
  ok &= preferences_.putString("wifissid", network_.wifiSsid) == strlen(network_.wifiSsid);
  ok &= preferences_.putString("wifipass", network_.wifiPassword) == strlen(network_.wifiPassword);
  ok &= preferences_.putString("mqtthost", network_.mqttServer) == strlen(network_.mqttServer);
  ok &= preferences_.putUInt("mqttport", network_.mqttPort) == sizeof(uint32_t);
  ok &= preferences_.putString("mqttuser", network_.mqttUsername) == strlen(network_.mqttUsername);
  ok &= preferences_.putString("mqttpass", network_.mqttPassword) == strlen(network_.mqttPassword);
  ok &= preferences_.putString("mqtttopic", network_.mqttBaseTopic) == strlen(network_.mqttBaseTopic);
  ok &= preferences_.putString("devicename", network_.deviceName) == strlen(network_.deviceName);
  ok &= preferences_.putUInt("heartms", network_.heartbeatIntervalMs) == sizeof(uint32_t);
  ok &= preferences_.putUInt("retrymin", network_.reconnectMinMs) == sizeof(uint32_t);
  ok &= preferences_.putUInt("retrymax", network_.reconnectMaxMs) == sizeof(uint32_t);
  if (ok) ok &= preferences_.putUInt("version", kConfigVersion) == sizeof(uint32_t);
  return ok;
}

bool ConfigManager::setLevelZero(float pitch, float roll) {
  if (!isfinite(pitch) || !isfinite(roll)) return false;
  const LevelConfig previous = level_;
  level_.pitchOffsetDeg = pitch;
  level_.rollOffsetDeg = roll;
  if (save()) return true;
  level_ = previous;
  return false;
}

bool ConfigManager::factoryReset() {
  if (!preferences_.clear()) return false;
  level_ = LevelConfig{};
  network_ = NetworkConfig{};
  return save();
}

bool ConfigManager::setValue(const String& key, const String& value, String& error) {
  LevelConfig level = level_;
  NetworkConfig network = network_;
  const bool isBool = key == "invert_pitch" || key == "invert_roll" || key == "swap_axes";
  const bool isText = key == "wifi_ssid" || key == "wifi_password" || key == "mqtt_server" ||
                      key == "mqtt_username" || key == "mqtt_password" || key == "mqtt_base_topic" || key == "device_name";
  const bool isInteger = key.endsWith("_ms") || key == "mqtt_port";
  double numeric = 0;
  if (!isBool && !isText) {
    char* end = nullptr;
    numeric = strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0' || !isfinite(numeric) || fabs(numeric) > 2147483647.0 ||
        (isInteger && (numeric < 0 || floor(numeric) != numeric))) {
      error = "expected a finite number";
      return false;
    }
  }
  const String text = value == "-" ? String("") : value;
  bool flag = false;
  if (key == "wifi_ssid") { if (!copyText(text, network.wifiSsid, error)) return false; }
  else if (key == "wifi_password") { if (!copyText(text, network.wifiPassword, error)) return false; }
  else if (key == "mqtt_server") { if (!copyText(text, network.mqttServer, error)) return false; }
  else if (key == "mqtt_username") { if (!copyText(text, network.mqttUsername, error)) return false; }
  else if (key == "mqtt_password") { if (!copyText(text, network.mqttPassword, error)) return false; }
  else if (key == "mqtt_base_topic") { if (!copyText(text, network.mqttBaseTopic, error)) return false; }
  else if (key == "device_name") { if (!copyText(text, network.deviceName, error)) return false; }
  else if (key == "mqtt_port") { if (numeric < 1 || numeric > 65535) { error = "mqtt_port must be 1..65535"; return false; } network.mqttPort = static_cast<uint16_t>(numeric); }
  else if (key == "heartbeat_interval_ms") network.heartbeatIntervalMs = static_cast<uint32_t>(numeric);
  else if (key == "reconnect_min_ms") network.reconnectMinMs = static_cast<uint32_t>(numeric);
  else if (key == "reconnect_max_ms") network.reconnectMaxMs = static_cast<uint32_t>(numeric);
  else if (key == "wheelbase_mm") level.wheelbaseMm = static_cast<float>(numeric);
  else if (key == "front_track_mm") level.frontTrackMm = static_cast<float>(numeric);
  else if (key == "rear_track_mm") level.rearTrackMm = static_cast<float>(numeric);
  else if (key == "track_width_mm") level.frontTrackMm = level.rearTrackMm = static_cast<float>(numeric);
  else if (key == "perfect_tolerance_deg") level.perfectToleranceDeg = static_cast<float>(numeric);
  else if (key == "acceptable_tolerance_deg") level.acceptableToleranceDeg = static_cast<float>(numeric);
  else if (key == "stable_variation_deg") level.stableVariationDeg = static_cast<float>(numeric);
  else if (key == "stable_duration_ms") level.stableDurationMs = static_cast<uint32_t>(numeric);
  else if (key == "invert_pitch") { if (!parseBool(value, flag)) { error = "expected true/false"; return false; } level.invertPitch = flag; }
  else if (key == "invert_roll") { if (!parseBool(value, flag)) { error = "expected true/false"; return false; } level.invertRoll = flag; }
  else if (key == "swap_axes") { if (!parseBool(value, flag)) { error = "expected true/false"; return false; } level.swapAxes = flag; }
  else { error = "unknown key"; return false; }

  if (!validate(level, error) || !validate(network, error)) return false;
  const LevelConfig oldLevel = level_;
  const NetworkConfig oldNetwork = network_;
  level_ = level;
  network_ = network;
  if (save()) return true;
  level_ = oldLevel;
  network_ = oldNetwork;
  error = "NVS write failed";
  return false;
}

void ConfigManager::print(Stream& output) const {
  output.printf("wheelbase_mm=%.1f\nfront_track_mm=%.1f\nrear_track_mm=%.1f\n", level_.wheelbaseMm, level_.frontTrackMm, level_.rearTrackMm);
  output.printf("pitch_offset_deg=%.4f\nroll_offset_deg=%.4f\n", level_.pitchOffsetDeg, level_.rollOffsetDeg);
  output.printf("invert_pitch=%s\ninvert_roll=%s\nswap_axes=%s\n", level_.invertPitch ? "true" : "false", level_.invertRoll ? "true" : "false", level_.swapAxes ? "true" : "false");
  output.printf("perfect_tolerance_deg=%.3f\nacceptable_tolerance_deg=%.3f\nstable_variation_deg=%.3f\nstable_duration_ms=%lu\n",
                level_.perfectToleranceDeg, level_.acceptableToleranceDeg, level_.stableVariationDeg, static_cast<unsigned long>(level_.stableDurationMs));
  output.printf("wifi_ssid=%s\nwifi_password=%s\nmqtt_server=%s\nmqtt_port=%u\nmqtt_username=%s\nmqtt_password=%s\n",
                network_.wifiSsid, network_.wifiPassword[0] ? "<set>" : "<empty>", network_.mqttServer, network_.mqttPort,
                network_.mqttUsername, network_.mqttPassword[0] ? "<set>" : "<empty>");
  output.printf("mqtt_base_topic=%s\ndevice_name=%s\nheartbeat_interval_ms=%lu\nreconnect_min_ms=%lu\nreconnect_max_ms=%lu\n",
                network_.mqttBaseTopic, network_.deviceName, static_cast<unsigned long>(network_.heartbeatIntervalMs),
                static_cast<unsigned long>(network_.reconnectMinMs), static_cast<unsigned long>(network_.reconnectMaxMs));
}

}  // namespace camper
