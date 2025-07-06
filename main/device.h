#include <driver/gpio.h>

// ESP-IDF core dependencies
#include "esp_event.h" // for esp_event_loop_create_default()
#include "nvs_flash.h" // for nvs_flash_init()
#include "esp_err.h"   // for esp_err_t and ESP_ERROR_CHECK

// esp-wifi-connect headers
#include "ssid_manager.h"          // SsidManager::GetInstance()
#include "wifi_configuration_ap.h" // WifiConfigurationAp::GetInstance()
#include "wifi_station.h"          // WifiStation::GetInstance()

constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_20;


// basic device initialization (NVS, event loop)
void device_init();

// start captive portal if needed, otherwise connect to Wi-Fi
void wifi_init();
