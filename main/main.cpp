
#include <iot_button.h>
#include <button_gpio.h>
#include <esp_log.h>
#include <esp_err.h>

static const char *TAG = "button_example";

static const int32_t BUTTON_GPIO_NUM = 0; // Replace with your button's GPIO number
#define BUTTON_ACTIVE_LEVEL 0 // Active low (pressed = LOW)

void button_init()
{
    const button_config_t btn_cfg = {
        .long_press_time = 1000,  // 1 second for long press
        .short_press_time = 50    // 50ms for short press
    };
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_GPIO_NUM,
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

// #define BUTTON_GPIO_NUM 0     // Replace with your button's GPIO number
// #define BUTTON_ACTIVE_LEVEL 1 // For pull-down, active level is HIGH

// // Callback for single click
// static void button_single_click_cb(void *arg, void *usr_data)
// {
//     ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
//     ESP_LOGI(TAG, "%s", "BUTTON_SINGLE_CLICK");
// }

// // Callback for double click
// static void button_double_click_cb(void *arg, void *usr_data)
// {
//     ESP_LOGI(TAG, "BUTTON_DOUBLE_CLICK");
//     ESP_LOGI(TAG, "%s", "BUTTON_DOUBLE_CLICK");
// }

// // Callback for long press
// static void button_long_press_cb(void *arg, void *usr_data)
// {
//     ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START");
//     ESP_LOGI(TAG, "%s", "BUTTON_LONG_PRESS_START");
// }

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Button example started");
    
    // Initialize the button
    button_init();

    // TODO: also consider low power mode: https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html#low-power
}
