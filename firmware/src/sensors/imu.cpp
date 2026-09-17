#include "imu.h"
#include <math.h>
#include <string.h>
namespace camper {
namespace {
constexpr uint8_t kWhoAmI = 0x75, kPower1 = 0x6B, kPower2 = 0x6C, kSampleDivider = 0x19;
constexpr uint8_t kConfig = 0x1A, kGyroConfig = 0x1B, kAccelConfig = 0x1C, kAccelConfig2 = 0x1D, kData = 0x3B;
constexpr float kRadToDeg = 57.2957795131f, kAlpha = 0.98f;
constexpr uint32_t kStaleMs = 1000;
constexpr uint16_t kFrozenLimit = 100;
int16_t readI16(const uint8_t* p) { return static_cast<int16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]); }
}
const char* Imu::typeName(ImuType type) { switch (type) { case ImuType::MPU6500: return "MPU-6500"; case ImuType::MPU9250: return "MPU-9250"; default: return "NONE"; } }
void Imu::setFault(const char* reason) { faultReason_ = reason; }
bool Imu::begin(TwoWire& wire, uint8_t sda, uint8_t scl) {
  wire_ = &wire;
  wire_->begin(sda, scl, 400000);
  wire_->setTimeOut(50);
  if (!detect() || !configure()) return false;
  delay(100);
  previousMicros_ = micros(); lastGoodMs_ = millis(); faultReason_ = "none";
  return true;
}
bool Imu::detect() {
  for (const uint8_t candidate : {0x68, 0x69}) {
    address_ = candidate;
    uint8_t identity = 0;
    if (!readRegister(kWhoAmI, identity)) continue;
    whoAmI_ = identity;
    if (identity == 0x70) { type_ = ImuType::MPU6500; return true; }
    if (identity == 0x71 || identity == 0x73) { type_ = ImuType::MPU9250; return true; }
  }
  address_ = 0; type_ = ImuType::NONE; setFault("WHO_AM_I mismatch or I2C device absent"); return false;
}
bool Imu::configure() {
  if (!writeRegister(kPower1, 0x80)) return false;
  delay(100);
  return writeRegister(kPower1, 0x01) && writeRegister(kPower2, 0x00) && writeRegister(kConfig, 0x03) &&
         writeRegister(kSampleDivider, 0x09) && writeRegister(kGyroConfig, 0x08) &&
         writeRegister(kAccelConfig, 0x08) && writeRegister(kAccelConfig2, 0x03);
}
bool Imu::readRegister(uint8_t reg, uint8_t& value) { return readRegisters(reg, &value, 1); }
bool Imu::writeRegister(uint8_t reg, uint8_t value) {
  wire_->beginTransmission(address_); wire_->write(reg); wire_->write(value);
  if (wire_->endTransmission(true) != 0) { setFault("I2C write failed"); return false; }
  return true;
}
bool Imu::readRegisters(uint8_t reg, uint8_t* data, size_t size) {
  wire_->beginTransmission(address_); wire_->write(reg);
  if (wire_->endTransmission(false) != 0) { setFault("I2C address/register failed"); return false; }
  if (wire_->requestFrom(address_, size, true) != size) { setFault("I2C short read"); return false; }
  for (size_t i = 0; i < size; ++i) data[i] = wire_->read();
  return true;
}
bool Imu::update(uint32_t nowMicros, uint32_t nowMs) {
  uint8_t bytes[14];
  if (!readRegisters(kData, bytes, sizeof(bytes))) { if (consecutiveErrors_ < UINT16_MAX) ++consecutiveErrors_; return false; }
  int16_t raw[6] = {readI16(&bytes[0]), readI16(&bytes[2]), readI16(&bytes[4]), readI16(&bytes[8]), readI16(&bytes[10]), readI16(&bytes[12])};
  if (memcmp(raw, previousRaw_, sizeof(raw)) == 0) { if (identicalFrames_ < UINT16_MAX) ++identicalFrames_; } else identicalFrames_ = 0;
  memcpy(previousRaw_, raw, sizeof(raw)); memcpy(reading_.raw, raw, sizeof(raw));
  if (identicalFrames_ >= kFrozenLimit) { setFault("IMU data frozen"); return false; }
  reading_.accelXG = raw[0] / 8192.0f; reading_.accelYG = raw[1] / 8192.0f; reading_.accelZG = raw[2] / 8192.0f;
  reading_.gyroXDps = raw[3] / 65.5f; reading_.gyroYDps = raw[4] / 65.5f; reading_.gyroZDps = raw[5] / 65.5f;
  const float accelRoll = atan2f(reading_.accelYG, reading_.accelZG) * kRadToDeg;
  const float accelPitch = atan2f(-reading_.accelXG, sqrtf(reading_.accelYG * reading_.accelYG + reading_.accelZG * reading_.accelZG)) * kRadToDeg;
  const float dt = static_cast<float>(nowMicros - previousMicros_) * 1.0e-6f;
  previousMicros_ = nowMicros;
  if (!filterInitialized_ || dt <= 0 || dt > 0.2f) {
    attitude_.rollDeg = accelRoll; attitude_.pitchDeg = accelPitch; filterInitialized_ = true;
  } else {
    attitude_.rollDeg = kAlpha * (attitude_.rollDeg + reading_.gyroXDps * dt) + (1.0f - kAlpha) * accelRoll;
    attitude_.pitchDeg = kAlpha * (attitude_.pitchDeg + reading_.gyroYDps * dt) + (1.0f - kAlpha) * accelPitch;
  }
  consecutiveErrors_ = 0; lastGoodMs_ = nowMs; faultReason_ = "none"; return true;
}
bool Imu::healthy(uint32_t nowMs) const {
  return type_ != ImuType::NONE && consecutiveErrors_ < 5 && identicalFrames_ < kFrozenLimit && (nowMs - lastGoodMs_) <= kStaleMs;
}
}  // namespace camper
