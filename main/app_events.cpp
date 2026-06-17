#include "app_events.h"

#include <freertos/FreeRTOS.h>

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

esp_err_t app_post_event(app_event_id_t id) {
  // Bounded wait so we never deadlock when posting from within the event loop
  // task itself (e.g. the Wi-Fi -> app-event translation handler in device.cpp).
  return esp_event_post(APP_EVENTS, id, nullptr, 0, pdMS_TO_TICKS(50));
}
