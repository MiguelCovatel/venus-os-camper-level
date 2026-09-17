#pragma once
#include <stdint.h>
namespace camper {
enum class StabilityState : uint8_t { MOVING, STABILIZING, STABLE };
class StabilityDetector {
 public:
  StabilityState update(float pitchDeg, float rollDeg, uint32_t nowMs, float maximumVariationDeg, uint32_t requiredStableMs);
  void reset();
  StabilityState state() const { return state_; }
  static const char* stateName(StabilityState state);
 private:
  StabilityState state_ = StabilityState::MOVING;
  float referencePitch_ = 0, referenceRoll_ = 0, lastPitch_ = 0, lastRoll_ = 0;
  uint32_t stableSinceMs_ = 0;
  bool initialized_ = false;
};
}  // namespace camper
