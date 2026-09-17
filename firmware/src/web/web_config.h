#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "config/config_manager.h"
#include "level/level_calculator.h"
#include "level/stability_detector.h"

namespace camper {

struct WebLevelStatus {
  bool valid = false;
  float orientedPitchDeg = 0.0f;
  float orientedRollDeg = 0.0f;
  float pitchDeg = 0.0f;
  float rollDeg = 0.0f;
  WheelValues practicalCorrectionMm;
  StabilityState stability = StabilityState::MOVING;
  LevelQuality quality = LevelQuality::UNLEVEL;
};

class WebConfig {
 public:
  void begin(ConfigManager& config, uint32_t nowMs);
  void update(uint32_t nowMs, const WebLevelStatus& status);
  bool takeLevelZeroApplied();
  const char* accessPointName() const { return apName_; }
  bool accessPointActive() const { return apActive_; }

 private:
  static constexpr uint32_t kFallbackDelayMs = 120000;
  static constexpr char kApPassword[] = "camperlevel";

  void registerRoutes();
  void startAccessPoint();
  bool networkConfigured() const;
  bool authorize();
  void handleRoot();
  void handleStatus();
  void handleNetworks();
  void handleSave();
  void handleLevelZero();
  void handleReboot();
  void handleUpdateFinished();
  void handleUpdateUpload();
  void handleCaptivePortal();
  String renderPage() const;
  String statusJson() const;
  static String htmlEscape(const char* value);
  static String jsonEscape(const String& value);
  static bool copyArg(const String& value, char* target, size_t capacity, String& error);
  static float parseFloatArg(const String& value, float fallback, bool& ok);
  static uint32_t parseUnsignedArg(const String& value, uint32_t fallback, bool& ok);

  ConfigManager* config_ = nullptr;
  WebServer server_{80};
  DNSServer dns_;
  WebLevelStatus status_;
  uint32_t wifiFallbackAtMs_ = 0;
  uint32_t rebootAtMs_ = 0;
  bool serverStarted_ = false;
  bool apActive_ = false;
  bool mdnsStarted_ = false;
  bool levelZeroApplied_ = false;
  bool updateAuthorized_ = false;
  bool updateFailed_ = false;
  char apName_[32] = "CamperLevel";
};

}  // namespace camper
