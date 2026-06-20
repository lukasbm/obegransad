#include "button.h"

#include "app_events.h"
#include "device.h" // BUTTON_PIN

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "button";

// Polling/timing parameters (milliseconds).
static constexpr uint32_t POLL_MS = 10;        // sample interval
static constexpr uint32_t DEBOUNCE_MS = 30;    // stable time before a level counts
static constexpr uint32_t LONG_PRESS_MS = 2000; // hold this long => long press
static constexpr uint32_t DOUBLE_GAP_MS = 300; // max gap between clicks => double

// Button is wired active-low (pressed pulls the pin to GND); the pin idles high
// thanks to the internal pull-up.
static inline bool raw_pressed() { return gpio_get_level(BUTTON_PIN) == 0; }

static inline uint32_t now_ms() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void button_task(void * /*arg*/) {
    bool stable = false;     // debounced pressed state
    bool last_raw = false;   // last sampled raw level
    uint32_t last_change = now_ms(); // when the raw level last changed
    uint32_t press_start = 0;
    bool long_fired = false; // long press already reported for this hold

    // A short click is held back until the double-click window passes, so we can
    // tell a single click from the first half of a double click.
    bool pending_click = false;
    uint32_t pending_since = 0;

    ESP_LOGI(TAG, "button task started on GPIO%d", (int)BUTTON_PIN);

    while (true) {
        const uint32_t now = now_ms();
        const bool raw = raw_pressed();

        if (raw != last_raw) {
            last_raw = raw;
            last_change = now;
        }

        // Commit a debounced transition once the raw level has been stable long
        // enough.
        if (raw != stable && (now - last_change) >= DEBOUNCE_MS) {
            stable = raw;
            if (stable) {
                press_start = now;
                long_fired = false;
                ESP_LOGD(TAG, "press down");
            } else {
                const uint32_t held = now - press_start;
                ESP_LOGD(TAG, "release after %ums", (unsigned)held);
                if (held >= LONG_PRESS_MS) {
                    // long press was already emitted on the threshold below
                } else if (pending_click && (now - pending_since) <= DOUBLE_GAP_MS) {
                    pending_click = false;
                    ESP_LOGI(TAG, "double press");
                    app_post_event(APP_EVT_BUTTON_DOUBLE);
                } else {
                    pending_click = true;
                    pending_since = now;
                }
            }
        }

        // Long press fires while still held, after the threshold.
        if (stable && !long_fired && (now - press_start) >= LONG_PRESS_MS) {
            long_fired = true;
            pending_click = false; // a long press is not a click
            ESP_LOGI(TAG, "long press");
            app_post_event(APP_EVT_BUTTON_LONG);
        }

        // Resolve a lone click once the double-click window has elapsed.
        if (pending_click && (now - pending_since) > DOUBLE_GAP_MS) {
            pending_click = false;
            ESP_LOGI(TAG, "short press");
            app_post_event(APP_EVT_BUTTON_SHORT);
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

esp_err_t button_init() {
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << BUTTON_PIN;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    if (xTaskCreate(button_task, "button", 3072, nullptr, 5, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "failed to create button task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
