#include "mqtt_transport.h"
#include <ctype.h>
#include <esp_system.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
namespace camper {
namespace { constexpr uint16_t kBufferBytes = 2048, kKeepAliveSeconds = 15, kSocketTimeoutSeconds = 1; }
MqttTransport::MqttTransport() : mqttClient_(wifiClient_) {
  mqttClient_.setCallback([this](char* topic, uint8_t* payload, unsigned int length) { handleMessage(topic, payload, length); });
}
bool MqttTransport::enabled(const NetworkConfig& c) { return c.wifiSsid[0] && c.mqttServer[0]; }
bool MqttTransport::sameConfig(const NetworkConfig& a, const NetworkConfig& b) {
  return !strcmp(a.wifiSsid, b.wifiSsid) && !strcmp(a.wifiPassword, b.wifiPassword) &&
         !strcmp(a.mqttServer, b.mqttServer) && a.mqttPort == b.mqttPort &&
         !strcmp(a.mqttUsername, b.mqttUsername) && !strcmp(a.mqttPassword, b.mqttPassword) &&
         !strcmp(a.mqttBaseTopic, b.mqttBaseTopic) && !strcmp(a.deviceName, b.deviceName) &&
         a.heartbeatIntervalMs == b.heartbeatIntervalMs && a.reconnectMinMs == b.reconnectMinMs && a.reconnectMaxMs == b.reconnectMaxMs;
}
void MqttTransport::begin(const NetworkConfig& config, uint32_t now) {
  snprintf(bootId_, sizeof(bootId_), "%08lX%08lX", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
  mqttClient_.setBufferSize(kBufferBytes); mqttClient_.setKeepAlive(kKeepAliveSeconds); mqttClient_.setSocketTimeout(kSocketTimeoutSeconds);
  wifiClient_.setTimeout(kSocketTimeoutSeconds); WiFi.persistent(false); WiFi.setAutoReconnect(false); WiFi.mode(WIFI_STA);
  applyConfiguration(config, now);
}
void MqttTransport::applyConfiguration(const NetworkConfig& config, uint32_t now) {
  if (mqttClient_.connected()) { publishRaw("availability", "offline", true); mqttClient_.disconnect(); }
  WiFi.disconnect(false, false); config_ = config;
  snprintf(clientId_, sizeof(clientId_), "%s-%08lX", config_.deviceName, static_cast<unsigned long>(ESP.getEfuseMac() & 0xFFFFFFFFULL));
  mqttClient_.setServer(config_.mqttServer, config_.mqttPort);
  wifiRetry_.begin(now, config_.reconnectMinMs, config_.reconnectMaxMs);
  mqttRetry_.begin(now, config_.reconnectMinMs, config_.reconnectMaxMs);
  nextPublishMs_ = now; wifiWasConnected_ = false; ntpStarted_ = false; diagnostics_ = MqttDiagnostics{};
  diagnostics_.state = enabled(config_) ? MqttConnectionState::WIFI_CONNECTING : MqttConnectionState::OFF;
}
bool MqttTransport::makeTopic(const char* suffix, char* output, size_t capacity) const {
  const int length = snprintf(output, capacity, "%s/%s", config_.mqttBaseTopic, suffix);
  return length > 0 && static_cast<size_t>(length) < capacity;
}
bool MqttTransport::connectMqtt(uint32_t now) {
  char willTopic[144];
  if (!makeTopic("availability", willTopic, sizeof(willTopic))) return false;
  const char* user = config_.mqttUsername[0] ? config_.mqttUsername : nullptr;
  const char* password = config_.mqttPassword[0] ? config_.mqttPassword : nullptr;
  const bool connected = mqttClient_.connect(clientId_, user, password, willTopic, 1, true, "offline");
  diagnostics_.mqttError = mqttClient_.state();
  if (!connected) { mqttRetry_.failed(now); return false; }
  const bool announced = publishRaw("availability", "online", true);
  char commandTopic[144];
  const bool subscribed = makeTopic("config/set/+", commandTopic, sizeof(commandTopic)) && mqttClient_.subscribe(commandTopic, 1);
  if (!announced || !subscribed) {
    mqttClient_.disconnect();
    mqttRetry_.failed(now);
    return false;
  }
  mqttRetry_.succeeded(now);
  nextPublishMs_ = now;
  return true;
}
void MqttTransport::handleMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (!configCommandHandler_ || !topic || !payload || !length || length >= 96) return;
  char prefix[144];
  if (!makeTopic("config/set/", prefix, sizeof(prefix))) return;
  const size_t prefixLength = strlen(prefix);
  if (strncmp(topic, prefix, prefixLength)) return;
  const char* key = topic + prefixLength;
  if (!*key || strchr(key, '/') || strlen(key) >= 48) return;
  for (const char* current = key; *current; ++current) {
    if (!(isalnum(static_cast<unsigned char>(*current)) || *current == '_')) return;
  }
  char value[96]; memcpy(value, payload, length); value[length] = '\0';
  String error;
  const bool accepted = configCommandHandler_(key, value, error);
  char ack[256];
  const int written = snprintf(ack, sizeof(ack),
      "{\"key\":\"%s\",\"accepted\":%s,\"value\":\"%s\",\"error\":\"%s\",\"uptime_ms\":%lu,\"boot_id\":\"%s\"}",
      key, accepted ? "true" : "false", value, accepted ? "" : error.c_str(), static_cast<unsigned long>(millis()), bootId_);
  if (written > 0 && static_cast<size_t>(written) < sizeof(ack)) publishRaw("config/ack", ack, false);
}
void MqttTransport::update(const NetworkConfig& config, uint32_t now) {
  if (!sameConfig(config_, config)) applyConfiguration(config, now);
  if (!enabled(config_)) { diagnostics_.state = MqttConnectionState::OFF; return; }
  if (WiFi.status() != WL_CONNECTED) {
    if (mqttClient_.connected()) mqttClient_.disconnect();
    wifiWasConnected_ = false; diagnostics_.state = MqttConnectionState::WIFI_CONNECTING; diagnostics_.rssiDbm = 0;
    if (wifiRetry_.due(now)) { WiFi.begin(config_.wifiSsid, config_.wifiPassword); wifiRetry_.failed(now); }
    return;
  }
  diagnostics_.rssiDbm = static_cast<int8_t>(WiFi.RSSI());
  if (!wifiWasConnected_) {
    wifiWasConnected_ = true; wifiRetry_.succeeded(now); mqttRetry_.makeDue(now);
    if (!ntpStarted_) { configTime(0, 0, "pool.ntp.org", "time.nist.gov"); ntpStarted_ = true; }
  }
  if (mqttClient_.connected() && mqttClient_.loop()) {
    diagnostics_.state = MqttConnectionState::ONLINE; diagnostics_.timeSynchronized = epochMillis() != 0; return;
  }
  diagnostics_.state = MqttConnectionState::MQTT_CONNECTING;
  if (mqttRetry_.due(now) && connectMqtt(now)) diagnostics_.state = MqttConnectionState::ONLINE;
  diagnostics_.timeSynchronized = epochMillis() != 0;
}
bool MqttTransport::shouldPublish(uint32_t now) { return mqttClient_.connected() && static_cast<int32_t>(now - nextPublishMs_) >= 0; }
uint32_t MqttTransport::beginBatch() { if (++sequence_ == 0) ++sequence_; return sequence_; }
bool MqttTransport::publishRaw(const char* suffix, const char* payload, bool retained) {
  if (!mqttClient_.connected()) return false;
  char topic[144]; return makeTopic(suffix, topic, sizeof(topic)) && mqttClient_.publish(topic, payload, retained);
}
bool MqttTransport::publishEnvelope(const char* suffix, const char* valueJson, uint64_t timestamp, uint32_t uptime, uint32_t sequence, const char* status) {
  char timestampText[24] = "null";
  if (timestamp) snprintf(timestampText, sizeof(timestampText), "%llu", static_cast<unsigned long long>(timestamp));
  char payload[384];
  const int length = snprintf(payload, sizeof(payload),
      "{\"value\":%s,\"timestamp_ms\":%s,\"uptime_ms\":%lu,\"sequence\":%lu,\"boot_id\":\"%s\",\"sensor_status\":\"%s\"}",
      valueJson, timestampText, static_cast<unsigned long>(uptime), static_cast<unsigned long>(sequence), bootId_, status);
  return length > 0 && static_cast<size_t>(length) < sizeof(payload) && publishRaw(suffix, payload, false);
}
void MqttTransport::finishBatch(uint32_t now, bool success) {
  nextPublishMs_ = now + config_.heartbeatIntervalMs;
  if (success) diagnostics_.lastPublishMs = now; else ++diagnostics_.publishFailures;
}
uint64_t MqttTransport::epochMillis() const {
  timeval current{};
  if (gettimeofday(&current, nullptr) != 0 || current.tv_sec < static_cast<time_t>(1700000000)) return 0;
  return static_cast<uint64_t>(current.tv_sec) * 1000ULL + static_cast<uint64_t>(current.tv_usec / 1000);
}
const char* MqttTransport::stateName(MqttConnectionState state) {
  switch (state) { case MqttConnectionState::OFF: return "DISABLED"; case MqttConnectionState::WIFI_CONNECTING: return "WIFI_CONNECTING"; case MqttConnectionState::MQTT_CONNECTING: return "MQTT_CONNECTING"; default: return "ONLINE"; }
}
}  // namespace camper
