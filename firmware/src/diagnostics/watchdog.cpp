#include "watchdog.h"
#include <esp_idf_version.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
namespace camper {
bool Watchdog::begin(uint32_t timeoutSeconds) {
#if ESP_IDF_VERSION_MAJOR >= 5
  const esp_task_wdt_config_t config = {.timeout_ms = timeoutSeconds * 1000, .idle_core_mask = 0, .trigger_panic = true};
  const esp_err_t initialized = esp_task_wdt_init(&config);
#else
  const esp_err_t initialized = esp_task_wdt_init(timeoutSeconds, true);
#endif
  if (initialized != ESP_OK && initialized != ESP_ERR_INVALID_STATE) return false;
  const esp_err_t added = esp_task_wdt_add(nullptr);
  return added == ESP_OK || added == ESP_ERR_INVALID_STATE;
}
void Watchdog::feed() { esp_task_wdt_reset(); }
const char* Watchdog::resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "POWER_ON"; case ESP_RST_EXT: return "EXTERNAL"; case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC"; case ESP_RST_INT_WDT: return "INTERRUPT_WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK_WATCHDOG"; case ESP_RST_WDT: return "OTHER_WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP"; case ESP_RST_BROWNOUT: return "BROWNOUT"; default: return "UNKNOWN";
  }
}
}  // namespace camper
