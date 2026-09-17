#pragma once
#include <stdint.h>
namespace camper {
struct WheelValues { float frontLeft = 0, frontRight = 0, rearLeft = 0, rearRight = 0; };
struct LevelResult { WheelValues relativeHeightMm; WheelValues correctionMm; };
enum class LevelQuality : uint8_t { LEVEL, SLIGHTLY_UNLEVEL, UNLEVEL };
class LevelCalculator {
 public:
  static LevelResult calculate(float pitchDeg, float rollDeg, float wheelbaseMm, float frontTrackMm, float rearTrackMm);
  static WheelValues practicalCorrection(const WheelValues& exactCorrectionMm, float stepMm = 10.0f);
  static LevelQuality quality(float pitchDeg, float rollDeg, float perfectToleranceDeg, float acceptableToleranceDeg);
  static LevelQuality qualityWithHysteresis(float pitchDeg, float rollDeg, float perfectToleranceDeg,
                                            float acceptableToleranceDeg, LevelQuality previous,
                                            float hysteresisDeg = 0.1f);
  static const char* qualityName(LevelQuality quality);
};
}  // namespace camper
