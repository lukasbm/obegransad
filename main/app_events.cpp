#include "app_events.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

// History ring, newest overwrites oldest. Guarded by a critical section: the
// event bus is posted to from several tasks and read from the HTTP server.
static app_event_record_t s_history[APP_EVENT_HISTORY];
static uint32_t s_seq = 0; // total number of events posted
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t app_post_event(app_event_id_t id) {
  taskENTER_CRITICAL(&s_lock);
  s_seq++;
  app_event_record_t *slot = &s_history[(s_seq - 1) % APP_EVENT_HISTORY];
  slot->seq = s_seq;
  slot->id = (int32_t)id;
  slot->uptime_ms = (uint32_t)(esp_timer_get_time() / 1000);
  taskEXIT_CRITICAL(&s_lock);

  // Bounded wait so we never deadlock when posting from within the event loop
  // task itself (e.g. the Wi-Fi -> app-event translation handler in device.cpp).
  return esp_event_post(APP_EVENTS, id, nullptr, 0, pdMS_TO_TICKS(50));
}

size_t app_events_since(uint32_t after_seq, app_event_record_t *out,
                        size_t max, uint32_t *latest_seq) {
  size_t copied = 0;
  taskENTER_CRITICAL(&s_lock);
  const uint32_t total = s_seq;
  const uint32_t retained = total < APP_EVENT_HISTORY ? total : APP_EVENT_HISTORY;
  // Walk oldest -> newest so callers get chronological order.
  for (uint32_t i = 0; i < retained && copied < max; i++) {
    const uint32_t seq = total - retained + 1 + i;
    const app_event_record_t *rec = &s_history[(seq - 1) % APP_EVENT_HISTORY];
    if (rec->seq != seq) {
      continue; // slot was never written (can happen while filling up)
    }
    if (seq > after_seq) {
      out[copied++] = *rec;
    }
  }
  taskEXIT_CRITICAL(&s_lock);
  if (latest_seq != nullptr) {
    *latest_seq = total;
  }
  return copied;
}

const char *app_event_name(int32_t id) {
  switch (id) {
  case APP_EVT_WIFI_CONNECTED:
    return "APP_EVT_WIFI_CONNECTED";
  case APP_EVT_WIFI_DISCONNECTED:
    return "APP_EVT_WIFI_DISCONNECTED";
  case APP_EVT_CAPTIVE_PORTAL_ACTIVE:
    return "APP_EVT_CAPTIVE_PORTAL_ACTIVE";
  case APP_EVT_BUTTON_SHORT:
    return "APP_EVT_BUTTON_SHORT";
  case APP_EVT_BUTTON_LONG:
    return "APP_EVT_BUTTON_LONG";
  case APP_EVT_BUTTON_DOUBLE:
    return "APP_EVT_BUTTON_DOUBLE";
  case APP_EVT_TIME_SYNCED:
    return "APP_EVT_TIME_SYNCED";
  case APP_EVT_WEATHER_DATA_READY:
    return "APP_EVT_WEATHER_DATA_READY";
  case APP_EVT_SCENE_ADVANCE:
    return "APP_EVT_SCENE_ADVANCE";
  case APP_EVT_ERROR_OCCURED:
    return "APP_EVT_ERROR_OCCURED";
  default:
    return "APP_EVT_UNKNOWN";
  }
}
