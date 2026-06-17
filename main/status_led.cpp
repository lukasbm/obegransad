#include "status_led.hpp"

#include "device.h"

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_log.h>

static const char *TAG = "status_led";

namespace {
constexpr uint32_t STATUS_LED_ON_LEVEL = 1;
constexpr uint32_t STATUS_LED_OFF_LEVEL = 0;

uint32_t wifi_connected_to_led_level(bool connected) {
  return connected ? STATUS_LED_OFF_LEVEL : STATUS_LED_ON_LEVEL;
}
} // namespace

esp_err_t status_led_init() {
  ESP_RETURN_ON_ERROR(gpio_reset_pin(STATUS_LED_PIN), TAG,
                      "Failed to reset status LED pin");
  ESP_RETURN_ON_ERROR(gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT), TAG,
                      "Failed to set status LED pin direction");

  // Default to ON until Wi-Fi reaches the connected state.
  ESP_RETURN_ON_ERROR(gpio_set_level(STATUS_LED_PIN,
                                     wifi_connected_to_led_level(false)),
                      TAG, "Failed to set initial status LED state");
  return ESP_OK;
}

void status_led_set_wifi_connected(bool connected) {
  const esp_err_t err =
      gpio_set_level(STATUS_LED_PIN, wifi_connected_to_led_level(connected));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to update status LED: %s", esp_err_to_name(err));
  }
}
