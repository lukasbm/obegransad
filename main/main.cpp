#include "esp_check.h"
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/projdefs.h>

#include "button.h"
#include "clock.h"
#include "device.h"
#include "ikea-obegransad-panel.h"
#include "scene_registry.hpp"
#include "status_led.hpp"
#include "weather.h"
#include "weather_client.h"
#include "state.h"
#include "sdkconfig.h"

static const char* TAG = "main";

extern "C" void app_main() {
  ESP_LOGI(TAG, "Startup");

  // Reduce WiFi debug spam before initialization
  esp_log_level_set("wifi", ESP_LOG_WARN);
  esp_log_level_set("WifiStation", ESP_LOG_INFO);
  esp_log_level_set("WifiConfigurationAp", ESP_LOG_INFO);

  // nvs, event loop, networking (creates the default event loop the app bus
  // and status LED subscribe to)
  ESP_ERROR_CHECK(device_init());

  // Early hardware status LED: ON until Wi-Fi connects. Subscribes to the
  // app event bus, so it must come after device_init().
  ESP_ERROR_CHECK(status_led_init());

  // the only button
  ESP_ERROR_CHECK(button_init());

  // Initialize WiFi - connects to the SSID configured in Kconfig
  wifi_init();

  // set up sntp and time zone (from Kconfig)
  ESP_ERROR_CHECK(clock_init(CONFIG_OBG_TIMEZONE));

  // background weather client (idles until a fetch is requested)
  ESP_ERROR_CHECK(weather_client_init());

  // setup and start panel
  static panel_config_t panel_config = {
      .latch_pin = (gpio_num_t)3,
      .clk_pin = (gpio_num_t)4,
      .di_pin = (gpio_num_t)5,
      .oe_pin = (gpio_num_t)6,
      .spi_host = SPI2_HOST, // Use SPI2 for better performance
      .spi_clock_speed_hz = 3 * 1000 * 1000, // 1 MHz SPI clock speed
      .gamma = 2.2
  };

  ESP_ERROR_CHECK(panel_init(panel_config));
  ESP_ERROR_CHECK(panel_timer_start());

  // Register scenes (single place — see scene_registry.hpp)
  register_all_scenes();

  // Init State Machine
  StateMachine::instance().init();

  while (true) {
    StateMachine::instance().update();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}