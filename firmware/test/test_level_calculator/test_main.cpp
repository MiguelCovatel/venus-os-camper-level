#include <unity.h>
#include <algorithm>
#include <cmath>
#include "level/level_calculator.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif
void setUp() {}
void tearDown() {}
namespace {
constexpr float kWheelbase = 3000.0f, kTrack = 1800.0f, kTolerance = 0.02f, kPi = 3.14159265358979323846f;
void normalized(const camper::WheelValues& w) {
  const float minimum = std::min(std::min(w.frontLeft, w.frontRight), std::min(w.rearLeft, w.rearRight));
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0.0f, minimum);
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, w.frontLeft);
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, w.frontRight);
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, w.rearLeft);
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, w.rearRight);
}
void level() {
  const auto r = camper::LevelCalculator::calculate(0, 0, kWheelbase, kTrack, kTrack);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0, r.correctionMm.frontLeft);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0, r.correctionMm.frontRight);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0, r.correctionMm.rearLeft);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0, r.correctionMm.rearRight); normalized(r.correctionMm);
}
void pitchPositive() {
  const auto r = camper::LevelCalculator::calculate(1, 0, kWheelbase, kTrack, kTrack);
  const float expected = std::tan(kPi / 180.0f) * kWheelbase;
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0, r.correctionMm.frontLeft);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, expected, r.correctionMm.rearLeft); normalized(r.correctionMm);
}
void pitchNegative() {
  const auto r = camper::LevelCalculator::calculate(-1, 0, kWheelbase, kTrack, kTrack);
  TEST_ASSERT_GREATER_THAN_FLOAT(0, r.correctionMm.frontLeft);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0, r.correctionMm.rearLeft); normalized(r.correctionMm);
}
void rollPositive() {
  const auto r = camper::LevelCalculator::calculate(0, 1, kWheelbase, kTrack, kTrack);
  const float expected = std::tan(kPi / 180.0f) * kTrack;
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0, r.correctionMm.frontLeft);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, expected, r.correctionMm.frontRight); normalized(r.correctionMm);
}
void rollNegative() {
  const auto r = camper::LevelCalculator::calculate(0, -1, kWheelbase, kTrack, kTrack);
  TEST_ASSERT_GREATER_THAN_FLOAT(0, r.correctionMm.frontLeft);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0, r.correctionMm.frontRight); normalized(r.correctionMm);
}
void combined() {
  const auto r = camper::LevelCalculator::calculate(0.8f, -0.6f, 3400, 1810, 1770);
  normalized(r.correctionMm);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0, r.correctionMm.frontRight);
  TEST_ASSERT_NOT_EQUAL(r.relativeHeightMm.frontLeft, r.relativeHeightMm.rearLeft);
}
void unequalTracks() {
  const auto r = camper::LevelCalculator::calculate(0, 1, 3000, 1800, 1600);
  TEST_ASSERT_GREATER_THAN_FLOAT(r.correctionMm.rearRight, r.correctionMm.frontRight);
  normalized(r.correctionMm);
}
void quality() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(camper::LevelQuality::LEVEL), static_cast<int>(camper::LevelCalculator::quality(.5f, -.1f, .5f, 1.0f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(camper::LevelQuality::SLIGHTLY_UNLEVEL), static_cast<int>(camper::LevelCalculator::quality(.51f, -1.0f, .5f, 1.0f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(camper::LevelQuality::UNLEVEL), static_cast<int>(camper::LevelCalculator::quality(.1f, -1.01f, .5f, 1.0f)));
}
void practicalCorrection() {
  camper::WheelValues exact;
  exact.frontLeft = 3.0f; exact.frontRight = 17.0f;
  exact.rearLeft = 24.0f; exact.rearRight = 38.0f;
  const auto practical = camper::LevelCalculator::practicalCorrection(exact);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 0.0f, practical.frontLeft);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 20.0f, practical.frontRight);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 20.0f, practical.rearLeft);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, 40.0f, practical.rearRight);
}
void qualityHysteresis() {
  using camper::LevelQuality;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LevelQuality::LEVEL),
      static_cast<int>(camper::LevelCalculator::qualityWithHysteresis(.55f, 0, .5f, 1.0f, LevelQuality::LEVEL, .1f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LevelQuality::SLIGHTLY_UNLEVEL),
      static_cast<int>(camper::LevelCalculator::qualityWithHysteresis(.65f, 0, .5f, 1.0f, LevelQuality::LEVEL, .1f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LevelQuality::UNLEVEL),
      static_cast<int>(camper::LevelCalculator::qualityWithHysteresis(.95f, 0, .5f, 1.0f, LevelQuality::UNLEVEL, .1f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LevelQuality::SLIGHTLY_UNLEVEL),
      static_cast<int>(camper::LevelCalculator::qualityWithHysteresis(.85f, 0, .5f, 1.0f, LevelQuality::UNLEVEL, .1f)));
}
}
int runTests() { UNITY_BEGIN(); RUN_TEST(level); RUN_TEST(pitchPositive); RUN_TEST(pitchNegative); RUN_TEST(rollPositive); RUN_TEST(rollNegative); RUN_TEST(combined); RUN_TEST(unequalTracks); RUN_TEST(quality); RUN_TEST(practicalCorrection); RUN_TEST(qualityHysteresis); return UNITY_END(); }
#ifdef ARDUINO
void setup() { delay(500); runTests(); } void loop() {}
#else
int main(int, char**) { return runTests(); }
#endif
