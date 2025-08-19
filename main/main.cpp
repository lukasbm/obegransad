
#include "esp_check.h"
#include "freertos/idf_additions.h"
#include <button_gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/projdefs.h>
#include <iot_button.h>

#include "clock.h"
#include "config.h"
#include "device.h"
#include "ikea-obegransad-panel.h"
#include "server.h"
#include "weather.h"

static const char *TAG = "main";

static void button_long_press(void *arg, void *usr_data) {
  ESP_LOGI(TAG, "Button long press detected");
}

static void button_short_press(void *arg, void *usr_data) {
  ESP_LOGI(TAG, "Button short press detected");
}

esp_err_t button_init() {
  // TODO: also consider low power mode:
  // https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html#low-power

  const button_config_t btn_cfg = {
      .long_press_time = 5000, // 1 second for long press
      .short_press_time = 50   // 50ms for short press
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
  ESP_RETURN_ON_ERROR(iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, nullptr,
                                             button_long_press, nullptr),
                      TAG, "Failed to register long press callback");

  return ESP_OK;
}

void advance_state_machine() {}

extern "C" void app_main() {
  ESP_LOGI(TAG, "Startup");

  // nvs, event loop, networking
  ESP_ERROR_CHECK(device_init());

  // load initial NVS settings
  ESP_ERROR_CHECK(nvs_read_settings(g_settings));

  // the only button
  ESP_ERROR_CHECK(button_init());

  // start captive portal if no Wi-Fi credentials are stored
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
      .spi_clock_speed_hz = 2 * 1000 * 1000, // 10 MHz
  };
  ESP_ERROR_CHECK(panel_init(&panel_config));
  ESP_ERROR_CHECK(panel_timer_start());

  // TEST PANEL
  ESP_LOGI(TAG, "Testing panel display");
  panel_clear();
  panel_setPixel(8, 10, PANEL_BRIGHTNESS_1);
  panel_setPixel(8, 8, PANEL_BRIGHTNESS_2);
  panel_setPixel(8, 6, PANEL_BRIGHTNESS_3);

  // START SERVER
  esp_err_t ret = start_webserver();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(ret));
    return;
  } else {
    ESP_LOGI(TAG, "Web server started successfully");
  }

  // TEST WEATHER
  // ESP_LOGI(TAG, "Fetching weather data");
  // WeatherData weather_data;
  // ret = fetch_weather(49, 11, weather_data);
  // if (ret == ESP_OK) {
  //   weather_data.print();
  // } else {
  //   ESP_LOGE(TAG, "Failed to fetch weather: %s", esp_err_to_name(ret));
  // }

  // main loop
  while (true) {
    advance_state_machine();

    // TEST TIME
    // struct tm timeinfo;
    // if (get_local_time(timeinfo)) {
    //   char time_str[32];
    //   strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
    //   ESP_LOGI(TAG, "Current time: %s", time_str);
    // } else {
    //   ESP_LOGW(TAG, "Failed to get local time");
    // }

    vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for scheduler
  }
}
