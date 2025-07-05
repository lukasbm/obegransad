#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PANEL_WIDTH 16
#define PANEL_HEIGHT 16
#define BIT_DEPTH 2

    typedef struct
    {
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
    void panel_commit(void);
    uint8_t *panel_get_framebuffer(void);

#ifdef __cplusplus
}
#endif