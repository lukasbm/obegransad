#include "esp_err.h"
#include <driver/gpio.h>

constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_20;

// basic device initialization (NVS, event loop)
esp_err_t device_init();

// start captive portal if needed, otherwise connect to Wi-Fi
void wifi_init();

// Check if Wi-Fi is connected
bool wifi_check();

void wifi_clear_credentials();

void start_captive_portal();
void stop_captive_portal();

void enter_light_sleep();
