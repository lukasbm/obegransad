#include "weather_client.h"

#include "app_events.h"
#include "config.h"
#include "device.h" // wifi_check()
#include "weather.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "weather_client";

static TaskHandle_t s_task = nullptr;

// Periodic fallback so data refreshes even without explicit requests.
static constexpr uint32_t FETCH_INTERVAL_MS = 20 * 60 * 1000; // 20 min

static bool location_configured() {
  return !(g_settings.weather_latitude == 0.0 &&
           g_settings.weather_longitude == 0.0);
}

static void do_fetch() {
  if (!location_configured()) {
    ESP_LOGW(TAG, "Weather location not configured; skipping fetch");
    return;
  }
  if (!wifi_check()) {
    ESP_LOGD(TAG, "No Wi-Fi; skipping weather fetch");
    return;
  }

  WeatherData data;
  // fetch_weather() populates the cache via weather_set() on success.
  esp_err_t err = fetch_weather(g_settings.weather_latitude,
                                g_settings.weather_longitude, data);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Weather updated");
    app_post_event(APP_EVT_WEATHER_DATA_READY);
  } else {
    // Keep the last good cached value; just log and wait for the next attempt.
    ESP_LOGW(TAG, "Weather fetch failed: %s", esp_err_to_name(err));
  }
}

static void weather_task(void * /*arg*/) {
  while (true) {
    // Block until a fetch is requested, or the periodic fallback elapses.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FETCH_INTERVAL_MS));
    do_fetch();
  }
}

esp_err_t weather_client_init() {
  if (s_task != nullptr) {
    return ESP_OK;
  }
  // ~6 KB stack: TLS handshake + the 5 KB HTTP response buffer in weather.cpp.
  BaseType_t ok =
      xTaskCreate(weather_task, "weather", 6144, nullptr, 5, &s_task);
  return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

void weather_client_request_fetch() {
  if (s_task != nullptr) {
    xTaskNotifyGive(s_task);
  }
}
