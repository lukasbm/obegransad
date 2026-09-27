#pragma once

#include "esp_err.h"
#include <stdint.h>

// Runtime configuration, persisted in NVS and exposed through the HTTP API
// (see docs/openapi.yaml). Kconfig values are the defaults for a fresh device.
//
// Call config_store_init() after NVS is ready; call config_store_apply() after
// the panel is initialized (it applies timezone and panel brightness).

esp_err_t config_store_init(void);

// Applies loaded values that need subsystems to exist: timezone and panel
// brightness. Safe to call more than once.
void config_store_apply(void);

const char *config_get_timezone(void);
esp_err_t config_set_timezone(const char *tz);

double config_get_latitude(void);
double config_get_longitude(void);
esp_err_t config_set_location(double latitude, double longitude);

int config_get_anniversary_day(void);
int config_get_anniversary_month(void);
esp_err_t config_set_anniversary(int day, int month);

uint8_t config_get_brightness(void);
esp_err_t config_set_brightness(uint8_t brightness);
