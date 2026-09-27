#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include <stddef.h>
#include <stdint.h>

// Application-level event bus.
//
// Events are posted onto the default esp_event loop (created in device_init())
// under the APP_EVENTS base. Low-level drivers (Wi-Fi, button) translate their
// raw events into these high-level events; consumers (state machine, status
// LED) subscribe instead of polling. See DEVELOPER.md "Event-driven state
// model".
//
// Every posted event is also recorded in a small history ring so external
// clients (the HTTP API, the simulator renderer) can observe what happened
// without subscribing to the event loop. See app_events_since().
ESP_EVENT_DECLARE_BASE(APP_EVENTS);

enum app_event_id_t {
  APP_EVT_WIFI_CONNECTED,        // station obtained an IP
  APP_EVT_WIFI_DISCONNECTED,     // station lost connectivity
  APP_EVT_CAPTIVE_PORTAL_ACTIVE, // reserved: portal removed, never posted
  APP_EVT_BUTTON_SHORT,
  APP_EVT_BUTTON_LONG,
  APP_EVT_BUTTON_DOUBLE,
  // Appended (do not reorder — handlers switch on these by value):
  APP_EVT_TIME_SYNCED,         // NTP completed a sync
  APP_EVT_WEATHER_DATA_READY,  // weather client fetched fresh data
  APP_EVT_SCENE_ADVANCE,       // dwell timer elapsed; advance to next scene
  APP_EVT_ERROR_OCCURED,       // unrecoverable error; app should reflect it
};

// Post an application event onto the default event loop. Safe to call from any
// task context, including from within another event handler.
esp_err_t app_post_event(app_event_id_t id);

// Number of events kept in the history ring.
#define APP_EVENT_HISTORY 16

typedef struct {
  uint32_t seq;       // 1-based sequence number (0 = empty slot)
  int32_t id;         // app_event_id_t
  uint32_t uptime_ms; // milliseconds since boot when the event was posted
} app_event_record_t;

// Copy up to `max` records newer than `after_seq` (oldest first). Returns the
// number of records copied and stores the newest sequence number in
// *latest_seq (0 when no event was ever posted).
size_t app_events_since(uint32_t after_seq, app_event_record_t *out,
                        size_t max, uint32_t *latest_seq);

// Human-readable name of an app_event_id_t (e.g. "APP_EVT_BUTTON_SHORT").
const char *app_event_name(int32_t id);
