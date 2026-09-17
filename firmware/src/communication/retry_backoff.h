#pragma once
#include <stdint.h>
namespace camper {
class RetryBackoff {
 public:
  void begin(uint32_t nowMs, uint32_t minimumMs, uint32_t maximumMs);
  bool due(uint32_t nowMs) const;
  void failed(uint32_t nowMs);
  void succeeded(uint32_t nowMs);
  void makeDue(uint32_t nowMs);
 private:
  uint32_t minimumMs_ = 1000, maximumMs_ = 60000, currentDelayMs_ = 1000, nextAttemptMs_ = 0;
};
}  // namespace camper
