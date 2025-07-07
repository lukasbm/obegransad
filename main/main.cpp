
#include "freertos/idf_additions.h"
#include <button_gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/projdefs.h>
#include <iot_button.h>

#include "device.h"
#include "ikea-obegransad-panel.h"

static const char *TAG = "main";

#define BUTTON_ACTIVE_LEVEL 0 // Active low (pressed = LOW)

void button_init() {
  // TODO: also consider low power mode:
  // https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html#low-power

  const button_config_t btn_cfg = {
      .long_press_time = 1000, // 1 second for long press
      .short_press_time = 50   // 50ms for short press
  };
  const button_gpio_config_t btn_gpio_cfg = {
      .gpio_num = BUTTON_PIN,
      .active_level = BUTTON_ACTIVE_LEVEL,
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
}

void advance_state_machine() {
  // This function would contain the logic to advance the state machine.
  // For now, it does nothing.
  // You can implement your state machine logic here.
  // For example, you might check button states or Wi-Fi connection status.
}

extern "C" void app_main() {
  ESP_LOGI(TAG, "Startup");

  device_init();
  button_init();
  // wifi_init();

  static panel_config_t panel_config = {
      .latch_pin = (gpio_num_t)1,
      .clk_pin = (gpio_num_t)2,
      .di_pin = (gpio_num_t)3,
      .oe_pin = (gpio_num_t)4,
      .spi_host = SPI2_HOST,                  // Use SPI2 for better performance
      .spi_clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
  };
  panel_init(&panel_config);
  panel_timer_start();
  // set up some example image
  panel_setPixel(8, 8, PANEL_BRIGHTNESS_2);

  while (true) {
    advance_state_machine();
    ESP_LOGI(TAG, "State machine advanced");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for scheduler
  }
}
