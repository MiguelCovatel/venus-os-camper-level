#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "config/config_types.h"
#include "retry_backoff.h"
namespace camper {
enum class MqttConnectionState : uint8_t { OFF, WIFI_CONNECTING, MQTT_CONNECTING, ONLINE };
struct MqttDiagnostics {
  MqttConnectionState state = MqttConnectionState::OFF;
  int8_t rssiDbm = 0;
  int mqttError = 0;
  uint32_t lastPublishMs = 0;
  uint32_t publishFailures = 0;
  bool timeSynchronized = false;
};
class MqttTransport {
 public:
  using ConfigCommandHandler = bool (*)(const char* key, const char* value, String& error);
  MqttTransport();
  void setConfigCommandHandler(ConfigCommandHandler handler) { configCommandHandler_ = handler; }
  void begin(const NetworkConfig& config, uint32_t nowMs);
  void update(const NetworkConfig& config, uint32_t nowMs);
  bool shouldPublish(uint32_t nowMs);
  uint32_t beginBatch();
  bool publishRaw(const char* suffix, const char* payload, bool retained = false);
  bool publishEnvelope(const char* suffix, const char* valueJson, uint64_t timestampMs,
                       uint32_t uptimeMs, uint32_t sequence, const char* sensorStatus);
  void finishBatch(uint32_t nowMs, bool successful);
  uint64_t epochMillis() const;
  const char* bootId() const { return bootId_; }
  const MqttDiagnostics& diagnostics() const { return diagnostics_; }
  static const char* stateName(MqttConnectionState state);
 private:
  static bool sameConfig(const NetworkConfig& left, const NetworkConfig& right);
  static bool enabled(const NetworkConfig& config);
  void applyConfiguration(const NetworkConfig& config, uint32_t nowMs);
  bool connectMqtt(uint32_t nowMs);
  bool makeTopic(const char* suffix, char* output, size_t capacity) const;
  void handleMessage(char* topic, uint8_t* payload, unsigned int length);
  WiFiClient wifiClient_;
  PubSubClient mqttClient_;
  NetworkConfig config_;
  RetryBackoff wifiRetry_, mqttRetry_;
  MqttDiagnostics diagnostics_;
  uint32_t nextPublishMs_ = 0, sequence_ = 0;
  bool wifiWasConnected_ = false, ntpStarted_ = false;
  ConfigCommandHandler configCommandHandler_ = nullptr;
  char bootId_[17] = "", clientId_[48] = "";
};
}  // namespace camper
