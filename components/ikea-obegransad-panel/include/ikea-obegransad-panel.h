#pragma once

#include "soc/gpio_num.h"
#include <driver/spi_master.h>
#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PANEL_WIDTH 16
#define PANEL_HEIGHT 16
#define BIT_DEPTH 4 // has to be between 1 and 8

// provide a simple set of nice brightness levels (manually selected)
typedef enum : uint8_t {
  PANEL_BRIGHTNESS_OFF = 0,
  PANEL_BRIGHTNESS_1 = 1,
  PANEL_BRIGHTNESS_2 = 3,
  PANEL_BRIGHTNESS_3 = 15,
} Brightness;

extern const Brightness brightness_levels[];

// panel configuration structure
typedef struct {
  gpio_num_t latch_pin;
  gpio_num_t clk_pin;
  gpio_num_t di_pin;
  gpio_num_t oe_pin;
  spi_host_device_t spi_host;
  int spi_clock_speed_hz;
  float gamma; // gamma correction factor for brightness scaling
} panel_config_t;

/**
 * @brief Initialize the IKEA Obegransad panel with given configuration
 * @param config Pointer to panel configuration structure
 * @return ESP_OK on success, error code on failure
 *
 * This function initializes the GPIO pins, SPI interface, and prepares the
 * framebuffer for the panel.
 */
esp_err_t panel_init(panel_config_t config);

/**
 * @brief Start the ESP timer for 500Hz display refresh
 * Must be called after panel_init() to begin automatic display updates
 */
esp_err_t panel_timer_start(void);

/**
 * @brief Stop the ESP timer to halt display refresh
 * LEDs will turn off when the timer is stopped
 */
esp_err_t panel_timer_stop(void);

/**
 * @brief Set the brightness of a specific pixel on the framebuffer
 * It will be applied on the next refresh cycle.
 */
void panel_setPixel(uint8_t row, uint8_t col, uint8_t brightness);

/**
 * @brief Fill the entire framebuffer with a specific brightness level
 * It will be applied on the next refresh cycle.
 */
void panel_fill(uint8_t brightness);

/**
 * @brief Clear the framebuffer by setting all pixels to off
 * Equivalent to calling panel_fill(PANEL_BRIGHTNESS_OFF)
 * It will be applied on the next refresh cycle.
 */
static inline void panel_clear(void) { panel_fill(PANEL_BRIGHTNESS_OFF); }

/**
 * @brief Set the global brightness level for the entire panel
 * This will scale all pixel brightness values by the given factor.
 * The value should be between 0 and 255, where 0 is off and 255 is full
 * brightness. It will be applied on the next refresh cycle.
 *
 * @note The gamma correction factor set in panel_init() will be applied to
 * the brightness level.
 *
 * @param brightness Global brightness level (0-255)
 */
void panel_set_global_brightness(uint8_t brightness);

/**
 * @brief Get the current global brightness level
 * Returns the last value set by panel_set_global_brightness()
 *
 * @return Current global brightness level (0-255)
 */
uint8_t panel_get_global_brightness(void);

/**
 * @brief Commit changes to the framebuffer
 * This marks the framebuffer as needing a refresh, so the next timer tick will
 * apply the changes.
 */
void panel_commit(void);

#ifdef __cplusplus
}
#endif
