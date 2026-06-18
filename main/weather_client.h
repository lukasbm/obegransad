#pragma once

#include "esp_err.h"

// Background weather client.
//
// Owns a FreeRTOS task that fetches weather from open-meteo and publishes it via
// weather_set() (see weather.h). It fetches on demand (weather_client_request_fetch)
// and on a slow periodic fallback. Scenes never call fetch_weather() directly;
// they read the cache via weather_get()/weather_is_valid().

// Create the background task. Safe to call once during startup (after
// device_init / nvs_read_settings). The task idles until a fetch is requested.
esp_err_t weather_client_init();

// Ask the client to fetch as soon as possible (non-blocking). Call on Wi-Fi
// connect, after time sync, or when the configured location changes.
void weather_client_request_fetch();
