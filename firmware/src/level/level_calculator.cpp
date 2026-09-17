#include "level_calculator.h"
#include <algorithm>
#include <cmath>
namespace camper {
namespace { constexpr float kDegreesToRadians = 0.01745329251994329577f; }
LevelResult LevelCalculator::calculate(float pitch, float roll, float wheelbase, float frontTrack, float rearTrack) {
  const float longitudinal = std::tan(pitch * kDegreesToRadians);
  const float transverse = std::tan(roll * kDegreesToRadians);
  LevelResult result;
  result.relativeHeightMm.frontLeft = longitudinal * wheelbase * 0.5f + transverse * frontTrack * 0.5f;
  result.relativeHeightMm.frontRight = longitudinal * wheelbase * 0.5f - transverse * frontTrack * 0.5f;
  result.relativeHeightMm.rearLeft = -longitudinal * wheelbase * 0.5f + transverse * rearTrack * 0.5f;
  result.relativeHeightMm.rearRight = -longitudinal * wheelbase * 0.5f - transverse * rearTrack * 0.5f;
  const float maximum = std::max(std::max(result.relativeHeightMm.frontLeft, result.relativeHeightMm.frontRight),
                                 std::max(result.relativeHeightMm.rearLeft, result.relativeHeightMm.rearRight));
  result.correctionMm.frontLeft = std::max(0.0f, maximum - result.relativeHeightMm.frontLeft);
  result.correctionMm.frontRight = std::max(0.0f, maximum - result.relativeHeightMm.frontRight);
  result.correctionMm.rearLeft = std::max(0.0f, maximum - result.relativeHeightMm.rearLeft);
  result.correctionMm.rearRight = std::max(0.0f, maximum - result.relativeHeightMm.rearRight);
  return result;
}
WheelValues LevelCalculator::practicalCorrection(const WheelValues& exact, float step) {
  if (!std::isfinite(step) || step <= 0.0f) step = 10.0f;
  auto rounded = [step](float value) {
    if (!std::isfinite(value) || value <= step * 0.5f) return 0.0f;
    return std::max(0.0f, std::round(value / step) * step);
  };
  WheelValues result;
  result.frontLeft = rounded(exact.frontLeft);
  result.frontRight = rounded(exact.frontRight);
  result.rearLeft = rounded(exact.rearLeft);
  result.rearRight = rounded(exact.rearRight);
  return result;
}
LevelQuality LevelCalculator::quality(float pitch, float roll, float perfect, float acceptable) {
  const float worst = std::max(std::fabs(pitch), std::fabs(roll));
  if (worst <= perfect) return LevelQuality::LEVEL;
  if (worst <= acceptable) return LevelQuality::SLIGHTLY_UNLEVEL;
  return LevelQuality::UNLEVEL;
}
LevelQuality LevelCalculator::qualityWithHysteresis(float pitch, float roll, float perfect, float acceptable,
                                                    LevelQuality previous, float hysteresis) {
  const float worst = std::max(std::fabs(pitch), std::fabs(roll));
  const float margin = std::max(0.0f, hysteresis);
  switch (previous) {
    case LevelQuality::LEVEL:
      if (worst <= perfect + margin) return LevelQuality::LEVEL;
      return worst <= acceptable + margin ? LevelQuality::SLIGHTLY_UNLEVEL : LevelQuality::UNLEVEL;
    case LevelQuality::SLIGHTLY_UNLEVEL:
      if (worst < std::max(0.0f, perfect - margin)) return LevelQuality::LEVEL;
      if (worst > acceptable + margin) return LevelQuality::UNLEVEL;
      return LevelQuality::SLIGHTLY_UNLEVEL;
    case LevelQuality::UNLEVEL:
    default:
      if (worst >= std::max(0.0f, acceptable - margin)) return LevelQuality::UNLEVEL;
      return worst < std::max(0.0f, perfect - margin) ? LevelQuality::LEVEL : LevelQuality::SLIGHTLY_UNLEVEL;
  }
}
const char* LevelCalculator::qualityName(LevelQuality quality) {
  switch (quality) { case LevelQuality::LEVEL: return "LEVEL"; case LevelQuality::SLIGHTLY_UNLEVEL: return "SLIGHTLY_UNLEVEL"; default: return "UNLEVEL"; }
}
}  // namespace camper
