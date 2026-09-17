#pragma once
#include <Arduino.h>
#include <Wire.h>
namespace camper {
enum class ImuType : uint8_t { NONE, MPU6500, MPU9250 };
struct ImuReading {
  float accelXG = 0, accelYG = 0, accelZG = 0;
  float gyroXDps = 0, gyroYDps = 0, gyroZDps = 0;
  int16_t raw[6]{};
};
struct Attitude { float pitchDeg = 0, rollDeg = 0; };
class Imu {
 public:
  bool begin(TwoWire& wire, uint8_t sda, uint8_t scl);
  bool update(uint32_t nowMicros, uint32_t nowMs);
  bool healthy(uint32_t nowMs) const;
  ImuType type() const { return type_; }
  uint8_t address() const { return address_; }
  uint8_t whoAmI() const { return whoAmI_; }
  const ImuReading& reading() const { return reading_; }
  const Attitude& attitude() const { return attitude_; }
  const char* faultReason() const { return faultReason_; }
  static const char* typeName(ImuType type);
 private:
  bool detect();
  bool configure();
  bool readRegister(uint8_t reg, uint8_t& value);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegisters(uint8_t reg, uint8_t* data, size_t size);
  void setFault(const char* reason);
  TwoWire* wire_ = nullptr;
  uint8_t address_ = 0, whoAmI_ = 0;
  ImuType type_ = ImuType::NONE;
  ImuReading reading_;
  Attitude attitude_;
  uint32_t previousMicros_ = 0, lastGoodMs_ = 0;
  uint16_t consecutiveErrors_ = 0, identicalFrames_ = 0;
  int16_t previousRaw_[6]{};
  bool filterInitialized_ = false;
  const char* faultReason_ = "not initialized";
};
}  // namespace camper
