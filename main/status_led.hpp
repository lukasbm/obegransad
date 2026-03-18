#pragma once

#include "esp_err.h"

// Initialize the discrete status LED GPIO early during boot.
esp_err_t status_led_init();

// LED ON while Wi-Fi is not connected, OFF once connected.
void status_led_set_wifi_connected(bool connected);
