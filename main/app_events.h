#pragma once

#include "esp_err.h"
#include "esp_event.h"

// Application-level event bus.
//
// Events are posted onto the default esp_event loop (created in device_init())
// under the APP_EVENTS base. Low-level drivers (Wi-Fi, button) translate their
// raw events into these high-level events; consumers (state machine, status
// LED) subscribe instead of polling. See DEVELOPER.md "Event-driven state
// model".
ESP_EVENT_DECLARE_BASE(APP_EVENTS);

enum app_event_id_t {
  APP_EVT_WIFI_CONNECTED,        // station obtained an IP
  APP_EVT_WIFI_DISCONNECTED,     // station lost connectivity
  APP_EVT_CAPTIVE_PORTAL_ACTIVE, // provisioning AP started
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
