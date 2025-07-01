
#include "iot_button.h"
#include "esp_log.h"
#include "esp_err.h"

#define BUTTON_GPIO_NUM 0     // Replace with your button's GPIO number
#define BUTTON_ACTIVE_LEVEL 1 // For pull-down, active level is HIGH

static const char *TAG = "button_example";

// Callback for single click
static void button_single_click_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
    ESP_LOGI(TAG, "%s", "BUTTON_SINGLE_CLICK");
}

// Callback for double click
static void button_double_click_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_DOUBLE_CLICK");
    ESP_LOGI(TAG, "%s", "BUTTON_DOUBLE_CLICK");
}

// Callback for long press
static void button_long_press_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START");
    ESP_LOGI(TAG, "%s", "BUTTON_LONG_PRESS_START");
}

void app_main(void)
{
    // Configure the button
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_GPIO_NUM,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };
    button_handle_t gpio_btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn);
    if (NULL == gpio_btn)
    {
        ESP_LOGE(TAG, "Button create failed");
        ESP_LOGE(TAG, "%s", "Button create failed");
    }

    // Register single click callback
    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, NULL, button_single_click_cb, NULL);

    // Register double click callback
    iot_button_register_cb(gpio_btn, BUTTON_DOUBLE_CLICK, NULL, button_double_click_cb, NULL);

    // Register long press callback (default long press time)
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_START, NULL, button_long_press_cb, NULL);

    // TODO: also consider low power mode: https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html#low-power
}
