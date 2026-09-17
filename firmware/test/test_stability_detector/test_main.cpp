#include <unity.h>
#include "level/stability_detector.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif
void setUp() {}
void tearDown() {}
namespace {
int state(camper::StabilityState value) { return static_cast<int>(value); }
void becomesStable() {
  camper::StabilityDetector detector;
  TEST_ASSERT_EQUAL_INT(state(camper::StabilityState::STABILIZING), state(detector.update(0, 0, 100, .15f, 3000)));
  TEST_ASSERT_EQUAL_INT(state(camper::StabilityState::STABILIZING), state(detector.update(.05f, -.04f, 2000, .15f, 3000)));
  TEST_ASSERT_EQUAL_INT(state(camper::StabilityState::STABLE), state(detector.update(.04f, -.03f, 3100, .15f, 3000)));
}
void motionRestartsWindow() {
  camper::StabilityDetector detector;
  detector.update(0, 0, 0, .1f, 1000);
  detector.update(0, 0, 1000, .1f, 1000);
  TEST_ASSERT_EQUAL_INT(state(camper::StabilityState::MOVING), state(detector.update(.3f, 0, 1100, .1f, 1000)));
  TEST_ASSERT_EQUAL_INT(state(camper::StabilityState::STABILIZING), state(detector.update(.31f, 0, 1200, .1f, 1000)));
  TEST_ASSERT_EQUAL_INT(state(camper::StabilityState::STABLE), state(detector.update(.32f, 0, 2200, .1f, 1000)));
}
void cumulativeDriftIsMotion() {
  camper::StabilityDetector detector;
  detector.update(0, 0, 0, .1f, 3000);
  detector.update(.06f, 0, 500, .1f, 3000);
  TEST_ASSERT_EQUAL_INT(state(camper::StabilityState::MOVING), state(detector.update(.12f, 0, 1000, .1f, 3000)));
}
void resetStartsAgain() {
  camper::StabilityDetector detector;
  detector.update(0, 0, 0, .1f, 500);
  detector.update(0, 0, 500, .1f, 500);
  detector.reset();
  TEST_ASSERT_EQUAL_INT(state(camper::StabilityState::STABILIZING), state(detector.update(0, 0, 600, .1f, 500)));
}
}
int runTests() { UNITY_BEGIN(); RUN_TEST(becomesStable); RUN_TEST(motionRestartsWindow); RUN_TEST(cumulativeDriftIsMotion); RUN_TEST(resetStartsAgain); return UNITY_END(); }
#ifdef ARDUINO
void setup() { delay(500); runTests(); } void loop() {}
#else
int main(int, char**) { return runTests(); }
#endif
