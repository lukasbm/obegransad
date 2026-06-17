#pragma once

#include "esp_err.h"

// Initialize the discrete status LED GPIO and subscribe it to Wi-Fi
// connectivity events on the application event bus. The LED is ON while Wi-Fi
// is not connected, and OFF once connected.
//
// Requires the default event loop to exist, so call after device_init().
esp_err_t status_led_init();
