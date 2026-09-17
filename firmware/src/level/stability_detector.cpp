#include "stability_detector.h"
#include <cmath>
namespace camper {
void StabilityDetector::reset() { state_ = StabilityState::MOVING; stableSinceMs_ = 0; initialized_ = false; }
StabilityState StabilityDetector::update(float pitch, float roll, uint32_t now, float variation, uint32_t duration) {
  if (!initialized_) {
    initialized_ = true;
    referencePitch_ = lastPitch_ = pitch;
    referenceRoll_ = lastRoll_ = roll;
    stableSinceMs_ = now;
    state_ = StabilityState::STABILIZING;
    return state_;
  }
  const bool outside = std::fabs(pitch - referencePitch_) > variation || std::fabs(roll - referenceRoll_) > variation;
  const bool sudden = std::fabs(pitch - lastPitch_) > variation || std::fabs(roll - lastRoll_) > variation;
  lastPitch_ = pitch;
  lastRoll_ = roll;
  if (outside || sudden) {
    referencePitch_ = pitch; referenceRoll_ = roll; stableSinceMs_ = now; state_ = StabilityState::MOVING; return state_;
  }
  if (state_ == StabilityState::MOVING) {
    stableSinceMs_ = now; referencePitch_ = pitch; referenceRoll_ = roll; state_ = StabilityState::STABILIZING;
  } else if ((now - stableSinceMs_) >= duration) {
    state_ = StabilityState::STABLE;
  }
  return state_;
}
const char* StabilityDetector::stateName(StabilityState state) {
  switch (state) { case StabilityState::MOVING: return "MOVING"; case StabilityState::STABILIZING: return "STABILIZING"; default: return "STABLE"; }
}
}  // namespace camper
