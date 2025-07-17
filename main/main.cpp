
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

void button_init() {
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
  esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Button create failed: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(TAG, "Button created successfully");

  // register callbacks
  ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, nullptr,
                                         button_short_press, nullptr));
  ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, nullptr,
                                         button_long_press, nullptr));
}

void advance_state_machine() {
  // This function would contain the logic to advance the state machine.
  // For now, it does nothing.
  // You can implement your state machine logic here.
  // For example, you might check button states or Wi-Fi connection status.
}

extern "C" void app_main() {
  ESP_LOGI(TAG, "Startup");

  ESP_ERROR_CHECK(device_init());
  button_init();
  wifi_init();
  clock_init("CET-1CEST,M3.5.0,M10.5.0/3"); // FIXME: get from config!

  static panel_config_t panel_config = {
      .latch_pin = (gpio_num_t)3,
      .clk_pin = (gpio_num_t)4,
      .di_pin = (gpio_num_t)5,
      .oe_pin = (gpio_num_t)6,
      .spi_host = SPI2_HOST,                 // Use SPI2 for better performance
      .spi_clock_speed_hz = 2 * 1000 * 1000, // 10 MHz
  };
  panel_init(&panel_config);
  panel_timer_start();

  // TEST PANEL
  ESP_LOGI(TAG, "Testing panel display");
  panel_clear();
  panel_setPixel(8, 8, PANEL_BRIGHTNESS_2);

  // TEST CONFIG (serialize and parse)
  Settings settings_full = {
      .brightness_day = 100,
      .brightness_night = 50,
      .off_hours = 0b00000000000000000000001111111111, // Off during night
      .weather_latitude = 49.0,
      .weather_longitude = 11.0,
      .timezone = "CET-1CEST,M3.5.0,M10.5.0/3",
      .anniversary_day = 14,
      .anniversary_month = 2};
  Settings settings_empty = {};

  // test serialize settings to JSON
  ESP_LOGI(TAG, "Serializing settings");
  char serialized[1000]; // Pre-allocated buffer for serialized JSON
  esp_err_t err = serialize_settings_json(serialized, settings_full);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to serialize settings: %s", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "Serialized settings: %s", serialized);

  // test serialize empty settings to JSON
  // serialize_settings_json(serialized, settings_empty);
  // if (serialized == nullptr) {
  //   ESP_LOGE(TAG, "Failed to serialize empty settings");
  //   return;
  // }
  // ESP_LOGI(TAG, "Serialized empty settings: %s", serialized);
  // free(serialized);

  // TODO: test read settings from NVS

  // vTaskDelay(pdMS_TO_TICKS(15000)); // init delay!! FIXME: remove

  // START SERVER
  // esp_err_t ret = start_webserver();
  // if (ret != ESP_OK) {
  //   ESP_LOGE(TAG, "Failed to start web server: %s",
  //   esp_err_to_name(ret)); return;
  // } else {
  //   ESP_LOGI(TAG, "Web server started successfully");
  // }

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
    struct tm timeinfo;
    if (get_local_time(timeinfo)) {
      char time_str[32];
      strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
      ESP_LOGI(TAG, "Current time: %s", time_str);
    } else {
      ESP_LOGW(TAG, "Failed to get local time");
    }

    vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for scheduler
  }
}
