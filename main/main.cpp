
#include "esp_check.h"
#include "freertos/idf_additions.h"
#include <button_gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/projdefs.h>
#include <iot_button.h>

#include "app_events.h"
#include "clock.h"
#include "config.h"
#include "device.h"
#include "ikea-obegransad-panel.h"
#include "scene_switcher.h"
#include "server.h"
#include "status_led.hpp"
#include "weather.h"
#include "state.h"
#include "scenes/scene_test.hpp"
#include "scenes/game_of_life.hpp"
#include "scenes/scene_snake.hpp"
#include "scenes/scene_weather.hpp"
#include "scenes/scene_clock.hpp"

static const char *TAG = "main";

static void button_long_press(void *arg, void *usr_data) {
  app_post_event(APP_EVT_BUTTON_LONG);
}

static void button_short_press(void *arg, void *usr_data) {
  app_post_event(APP_EVT_BUTTON_SHORT);
}

static void button_double_press(void *arg, void *usr_data) {
  app_post_event(APP_EVT_BUTTON_DOUBLE);
}

esp_err_t button_init() {
  // TODO: also consider low power mode:
  // https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html#low-power

  const button_config_t btn_cfg = {
      .long_press_time = 5000,  // ms
      .short_press_time = 50   // ms
  };
  const button_gpio_config_t btn_gpio_cfg = {
      .gpio_num = BUTTON_PIN,
      .active_level = 0,         // Active low (pressed = LOW)
      .enable_power_save = true, // Enable power saving mode
      .disable_pull = false      // Enable internal pull-up
  };

  button_handle_t btn;
  ESP_RETURN_ON_ERROR(iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn),
                      TAG, "Failed to create button");

  // register callbacks
  ESP_RETURN_ON_ERROR(iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, nullptr,
                                             button_short_press, nullptr),
                      TAG, "Failed to register short press callback");
  ESP_RETURN_ON_ERROR(iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, nullptr,
                                             button_long_press, nullptr),
                      TAG, "Failed to register long press callback");
  ESP_RETURN_ON_ERROR(iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, nullptr,
                                             button_double_press, nullptr),
                      TAG, "Failed to register double press callback");

  return ESP_OK;
}

// Global scene instances
ClockScene clock_scene;
WeatherScene weather_scene;
GameOfLifeScene game_of_life_scene;
SnakeScene snake_scene;
SpriteTestScene test_scene;

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

  // load initial NVS settings
  ESP_ERROR_CHECK(nvs_read_settings(g_settings));

  // the only button
  ESP_ERROR_CHECK(button_init());

  // Initialize WiFi - starts captive portal if no credentials, otherwise connects
  wifi_init();

  // set up sntp and time zone
  // FIXME: get from config!
  ESP_ERROR_CHECK(clock_init("CET-1CEST,M3.5.0,M10.5.0/3"));

  // setup and start panel
  static panel_config_t panel_config = {
      .latch_pin = (gpio_num_t)3,
      .clk_pin = (gpio_num_t)4,
      .di_pin = (gpio_num_t)5,
      .oe_pin = (gpio_num_t)6,
      .spi_host = SPI2_HOST,                 // Use SPI2 for better performance
      .spi_clock_speed_hz = 3 * 1000 * 1000, // 1 MHz SPI clock speed
      .gamma = 2.2};

  ESP_ERROR_CHECK(panel_init(panel_config));
  ESP_ERROR_CHECK(panel_timer_start());
  
  // Register scenes
  register_scene(&clock_scene);
  register_scene(&weather_scene);
  register_scene(&game_of_life_scene);
  register_scene(&snake_scene);
  register_scene(&test_scene);

  // Init State Machine
  StateMachine::instance().init();

  while (true) {
    StateMachine::instance().update();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
