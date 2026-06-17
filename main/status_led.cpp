#include "status_led.hpp"

#include "app_events.h"
#include "device.h"

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_event.h>
#include <esp_log.h>

static const char *TAG = "status_led";

namespace {
constexpr uint32_t STATUS_LED_ON_LEVEL = 1;
constexpr uint32_t STATUS_LED_OFF_LEVEL = 0;

uint32_t wifi_connected_to_led_level(bool connected) {
  return connected ? STATUS_LED_OFF_LEVEL : STATUS_LED_ON_LEVEL;
}

void set_led(bool connected) {
  const esp_err_t err =
      gpio_set_level(STATUS_LED_PIN, wifi_connected_to_led_level(connected));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to update status LED: %s", esp_err_to_name(err));
  }
}

void on_app_event(void * /*arg*/, esp_event_base_t /*base*/, int32_t id,
                  void * /*data*/) {
  switch (id) {
  case APP_EVT_WIFI_CONNECTED:
    set_led(true);
    break;
  case APP_EVT_WIFI_DISCONNECTED:
  case APP_EVT_CAPTIVE_PORTAL_ACTIVE:
    set_led(false);
    break;
  default:
    break;
  }
}
} // namespace

esp_err_t status_led_init() {
  ESP_RETURN_ON_ERROR(gpio_reset_pin(STATUS_LED_PIN), TAG,
                      "Failed to reset status LED pin");
  ESP_RETURN_ON_ERROR(gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT), TAG,
                      "Failed to set status LED pin direction");

  // Default to ON until Wi-Fi reaches the connected state.
  ESP_RETURN_ON_ERROR(
      gpio_set_level(STATUS_LED_PIN, wifi_connected_to_led_level(false)), TAG,
      "Failed to set initial status LED state");

  ESP_RETURN_ON_ERROR(
      esp_event_handler_register(APP_EVENTS, ESP_EVENT_ANY_ID, &on_app_event,
                                 nullptr),
      TAG, "Failed to register status LED event handler");
  return ESP_OK;
}
