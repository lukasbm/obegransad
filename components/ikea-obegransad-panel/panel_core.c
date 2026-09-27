#include "panel_internal.h"

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

// Period of one refresh tick: 4 planes x 2000us = 8ms, i.e. a ~125Hz
// full-brightness cycle (see panel_hw.c for the plane on-times).
#define FRAME_PERIOD_US 2000

static const char *TAG = "panel";

const Brightness brightness_levels[] = {PANEL_BRIGHTNESS_OFF,
                                        PANEL_BRIGHTNESS_1, PANEL_BRIGHTNESS_2,
                                        PANEL_BRIGHTNESS_3};

// Core state
static panel_config_t g_config;
static uint8_t g_framebuffer[PANEL_HEIGHT][PANEL_WIDTH];
static uint8_t g_brightness_panel = 255; // gamma-corrected, read by the backend
static uint8_t g_brightness_user = 255;  // last value set by the user
static volatile bool g_refresh_needed = false;
static esp_timer_handle_t g_refresh_timer;
static TaskHandle_t g_panel_task_handle = NULL;

// Forward declarations
static void refresh_timer_callback(void *arg);
static void panel_task_func(void *arg);

//////////////////////////////////
// backend accessors            //
//////////////////////////////////

const uint8_t *panel_core_framebuffer(void) { return &g_framebuffer[0][0]; }

uint8_t panel_core_brightness(void) { return g_brightness_panel; }

//////////////////////////////////
// important API functions      //
//////////////////////////////////

esp_err_t panel_init(panel_config_t config) {
  ESP_LOGI(TAG, "Initializing IKEA Obegränsad panel driver");
  g_config = config;

  // Initialize framebuffer to zero (all LEDs off)
  memset(g_framebuffer, 0, sizeof(g_framebuffer));

  // Hardware (or simulator) backend
  ESP_RETURN_ON_ERROR(panel_backend_init(&g_config), TAG,
                      "panel backend initialization failed");

  // Create high priority task pinned to the last core (app core on dual-core,
  // core 0 on the single-core ESP32-C3). Pinning to a non-existent core 1
  // asserts in xTaskCreatePinnedToCore on single-core targets.
  xTaskCreatePinnedToCore(panel_task_func, "panel_task", 4096, NULL,
                          configMAX_PRIORITIES - 1, &g_panel_task_handle,
                          configNUMBER_OF_CORES - 1);

  // ESP Timer for refresh ticks: ISR-dispatched on chip targets for low
  // jitter; on the host target the shim runs callbacks in task context.
  esp_timer_create_args_t timer_args = {
      .callback = refresh_timer_callback,
      .name = "panel_refresh",
#if CONFIG_IDF_TARGET_LINUX
      .dispatch_method = ESP_TIMER_TASK,
#else
      .dispatch_method = ESP_TIMER_ISR,
#endif
  };
  ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &g_refresh_timer), TAG,
                      "ESP Timer creation failed");

  ESP_LOGI(TAG, "Panel driver initialized successfully - ready for refresh");
  return ESP_OK;
}

/**
 * @brief Start the ESP timer for display refresh
 * Must be called after panel_init() to begin automatic display updates
 */
esp_err_t panel_timer_start(void) {
  return esp_timer_start_periodic(g_refresh_timer, FRAME_PERIOD_US);
}

/**
 * @brief Stop the ESP timer to halt display refresh
 * LEDs will remain in their current state until timer is restarted
 */
esp_err_t panel_timer_stop(void) { return esp_timer_stop(g_refresh_timer); }

///////////////////////////////
// Framebuffer manipulation API //
///////////////////////////////

/**
 * @brief Maps a full 8 bit value (0-255) to the nearest supported brightness
 * level based on BIT_DEPTH
 */
static uint8_t map_value(uint8_t value) {
  if (value == 0) {
    return 0;
  } else if (value <= PANEL_BRIGHTNESS_1) {
    return PANEL_BRIGHTNESS_1;
  } else if (value <= PANEL_BRIGHTNESS_2) {
    return PANEL_BRIGHTNESS_2;
  } else {
    return PANEL_BRIGHTNESS_3;
  }
}

/**
 * @brief Set individual pixel brightness using logical coordinates
 * @param row Pixel row (0-15)
 * @param col Pixel column (0-15)
 * @param brightness Brightness level (see Brightness enum)
 */
void panel_setPixel(uint8_t row, uint8_t col, uint8_t brightness) {
  if (row >= PANEL_HEIGHT || col >= PANEL_WIDTH)
    return;
  g_framebuffer[row][col] = map_value(brightness);
  g_refresh_needed = true;
}

/**
 * @brief Fill entire panel with uniform brightness
 * @param brightness Brightness level (see Brightness enum)
 */
void panel_fill(uint8_t brightness) {
  memset(g_framebuffer, map_value(brightness), sizeof(g_framebuffer));
  g_refresh_needed = true;
}

/**
 * @brief Mark framebuffer as needing refresh (call after direct framebuffer
 * changes)
 */
void panel_commit(void) { g_refresh_needed = true; }

/**
 * @brief Set global brightness scaling factor
 * @param brightness Global brightness (0-255), applied to all pixels
 */
void panel_set_global_brightness(uint8_t brightness) {
  g_brightness_user = brightness;

  // Apply gamma correction
  float brightness_f = (float)brightness / 255.0f;
  brightness = (uint8_t)(pow(brightness_f, g_config.gamma) * 255.0f +
                         0.1f); // +0.1f for rounding

  g_brightness_panel = brightness;
  g_refresh_needed = true; // Trigger refresh with new brightness
}

/**
 * @brief Get current global brightness scaling factor
 * @return Current global brightness (0-255)
 */
uint8_t panel_get_global_brightness(void) { return g_brightness_user; }

//////////////////////////
// Refresh loop         //
//////////////////////////

/**
 * @brief ESP Timer callback - ISR context
 * Triggers the high-priority task to perform the actual refresh
 * @param arg User-defined argument (unused)
 */
static void IRAM_ATTR refresh_timer_callback(void *arg) {
#if CONFIG_IDF_TARGET_LINUX
  // The host esp_timer shim runs callbacks in task context, so the ISR-safe
  // notification must not be used there.
  xTaskNotifyGive(g_panel_task_handle);
#else
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(g_panel_task_handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif
}

/**
 * @brief Dedicated High Priority Task for Display Refresh
 * waits for notification from ISR timer and executes refresh logic
 */
static void panel_task_func(void *arg) {
  while (1) {
    // Wait for notification from timer ISR
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Rebuild backend data (bit planes on hardware) if the framebuffer changed
    if (g_refresh_needed) {
      g_refresh_needed = false;
      panel_backend_rebuild();
    }

    // Display each plane with precise timing (hardware) or publish the frame
    // to the simulator link.
    panel_backend_display();
  }
}
