
#include <iot_button.h>
#include <button_gpio.h>
#include <esp_log.h>
#include <esp_err.h>

#include "device.h"

static const char *TAG = "button_example";

#define BUTTON_ACTIVE_LEVEL 0 // Active low (pressed = LOW)

void button_init()
{
    // TODO: also consider low power mode: https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html#low-power

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
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Button create failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Button created successfully");
}

void advance_state_machine()
{
    // This function would contain the logic to advance the state machine.
    // For now, it does nothing.
    // You can implement your state machine logic here.
    // For example, you might check button states or Wi-Fi connection status.
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Startup");

    device_init();
    button_init();
    wifi_init();
    // panel_init();

    while (true)
    {
        advance_state_machine();

        vTaskDelay(pdMS_TO_TICKS(10)); // Delay for scheduler
    }
}
