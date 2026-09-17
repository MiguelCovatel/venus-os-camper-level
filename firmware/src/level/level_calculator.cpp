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
LevelQuality LevelCalculator::quality(float pitch, float roll, float perfect, float acceptable) {
  const float worst = std::max(std::fabs(pitch), std::fabs(roll));
  if (worst <= perfect) return LevelQuality::LEVEL;
  if (worst <= acceptable) return LevelQuality::SLIGHTLY_UNLEVEL;
  return LevelQuality::UNLEVEL;
}
const char* LevelCalculator::qualityName(LevelQuality quality) {
  switch (quality) { case LevelQuality::LEVEL: return "LEVEL"; case LevelQuality::SLIGHTLY_UNLEVEL: return "SLIGHTLY_UNLEVEL"; default: return "UNLEVEL"; }
}
}  // namespace camper
