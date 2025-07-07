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
#define BIT_DEPTH 2

typedef enum {
  PANEL_BRIGHTNESS_OFF = 0,
  PANEL_BRIGHTNESS_1 = 1,
  PANEL_BRIGHTNESS_2 = 2,
  PANEL_BRIGHTNESS_3 = 3,
} Brightness;

typedef struct {
  gpio_num_t latch_pin;
  gpio_num_t clk_pin;
  gpio_num_t di_pin;
  gpio_num_t oe_pin;
  spi_host_device_t spi_host;
  int spi_clock_speed_hz;
} panel_config_t;

esp_err_t panel_init(const panel_config_t *config);

void panel_timer_start(void);

void panel_timer_stop(void);

void panel_setPixel(uint8_t row, uint8_t col, Brightness brightness);

void panel_fill(Brightness brightness);

static inline void panel_clear(void) { panel_fill(PANEL_BRIGHTNESS_OFF); }

void panel_set_global_brightness(uint8_t brightness);
uint8_t panel_get_global_brightness(void);

void panel_commit(void);

#ifdef __cplusplus
}
#endif
