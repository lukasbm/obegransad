#pragma once

#include "esp_err.h"
#include <driver/gpio.h>

constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_20;
constexpr gpio_num_t STATUS_LED_PIN = GPIO_NUM_10;

// basic device initialization (NVS, event loop)
esp_err_t device_init();

// start captive portal if needed, otherwise connect to Wi-Fi
void wifi_init();

// Check if Wi-Fi is connected
bool wifi_check();

// Wait for WiFi connection with timeout (in milliseconds)
// Returns true if connected, false if timeout
bool wifi_wait_for_connection(uint32_t timeout_ms);

void wifi_clear_credentials();

void start_captive_portal();
void stop_captive_portal();
bool is_captive_portal_active();

bool wifi_has_credentials();

void enter_light_sleep();
