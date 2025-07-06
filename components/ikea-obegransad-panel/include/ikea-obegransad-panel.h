#pragma once

#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t gBright; // global brightness (0-255) (only scaling)

enum Brightness {
  PANEL_BRIGHTNESS_OFF,
  PANEL_BRIGHTNESS_1,
  PANEL_BRIGHTNESS_2,
  PANEL_BRIGHTNESS_3,
};

typedef struct {
  int latch_pin;
  int clk_pin;
  int di_pin;
  int oe_pin;
  int spi_host;
  int spi_clock_speed_hz;
  int rmt_channel;
  int timer_group;
  int timer_idx;
} panel_config_t;

esp_err_t panel_init(const panel_config_t *config);

void panel_timer_start(void);

void panel_timer_stop(void);

void panel_setPixel(uint8_t row, uint8_t col, Brightness brightness);

void panel_fill(Brightness col);

inline void panel_clear() { panel_fill(PANEL_BRIGHTNESS_OFF); }

uint8_t *panel_get_framebuffer(void);

void panel_commit(void);

#ifdef __cplusplus
}
#endif
