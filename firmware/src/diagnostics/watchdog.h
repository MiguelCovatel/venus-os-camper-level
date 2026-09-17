#pragma once
#include <Arduino.h>
namespace camper {
class Watchdog { public: static bool begin(uint32_t timeoutSeconds = 8); static void feed(); static const char* resetReason(); };
}  // namespace camper
