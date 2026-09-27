#pragma once

#include "esp_err.h"
#include "sdkconfig.h"

#if !CONFIG_IDF_TARGET_LINUX
#include <driver/gpio.h>

constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_20;
constexpr gpio_num_t STATUS_LED_PIN = GPIO_NUM_10;
#endif

// basic device initialization (NVS, event loop)
esp_err_t device_init();

// Connect to the Wi-Fi network configured in Kconfig. There is no captive
// portal (removed, see docs/known-issues.md): with an empty SSID the device
// stays offline until it is reflashed with credentials.
void wifi_init();

// Check if Wi-Fi is connected
bool wifi_check();

/**
 * @brief Debounces disconnects and forces reconnect attempts with a backoff.
 * Call periodically from the main loop; cheap, and a no-op while connected.
 */
void wifi_supervisor_tick();

// Wait for WiFi connection with timeout (in milliseconds)
// Returns true if connected, false if timeout
bool wifi_wait_for_connection(uint32_t timeout_ms);

void wifi_clear_credentials();

bool wifi_has_credentials();

void enter_light_sleep();
