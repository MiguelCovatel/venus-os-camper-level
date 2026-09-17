#include "retry_backoff.h"
#include <algorithm>
namespace camper {
void RetryBackoff::begin(uint32_t now, uint32_t minimum, uint32_t maximum) { minimumMs_ = minimum; maximumMs_ = std::max(minimum, maximum); currentDelayMs_ = minimum; nextAttemptMs_ = now; }
bool RetryBackoff::due(uint32_t now) const { return static_cast<int32_t>(now - nextAttemptMs_) >= 0; }
void RetryBackoff::failed(uint32_t now) { nextAttemptMs_ = now + currentDelayMs_; currentDelayMs_ = static_cast<uint32_t>(std::min<uint64_t>(static_cast<uint64_t>(currentDelayMs_) * 2, maximumMs_)); }
void RetryBackoff::succeeded(uint32_t now) { currentDelayMs_ = minimumMs_; nextAttemptMs_ = now; }
void RetryBackoff::makeDue(uint32_t now) { nextAttemptMs_ = now; }
}  // namespace camper
