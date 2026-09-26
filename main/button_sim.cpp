// Simulator button backend: no GPIO. Button presses arrive as command bytes
// from the host renderer over the simulator link (keys / on-screen buttons)
// and are posted as the same app events the hardware driver emits.

#include "sdkconfig.h"

#if !CONFIG_OBG_BUTTON_BACKEND_HW

#include "button.h"

#include "app_events.h"
#include "obegransad-sim.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "button_sim";

static void button_task(void * /*arg*/) {
  ESP_LOGI(TAG, "button task started (simulated, use the renderer keys)");

  while (true) {
    int cmd;
    while ((cmd = sim_link_poll_command()) != 0) {
      switch (cmd) {
      case 'S':
        ESP_LOGI(TAG, "short press (simulated)");
        app_post_event(APP_EVT_BUTTON_SHORT);
        break;
      case 'D':
        ESP_LOGI(TAG, "double press (simulated)");
        app_post_event(APP_EVT_BUTTON_DOUBLE);
        break;
      case 'L':
        ESP_LOGI(TAG, "long press (simulated)");
        app_post_event(APP_EVT_BUTTON_LONG);
        break;
      default:
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

esp_err_t button_init() {
  if (xTaskCreate(button_task, "button", 3072, nullptr, 5, nullptr) != pdPASS) {
    ESP_LOGE(TAG, "failed to create button task");
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

#endif // !CONFIG_OBG_BUTTON_BACKEND_HW
