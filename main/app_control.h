#pragma once

#include "esp_err.h"
#include <stdint.h>

// Runtime control actions.
//
// Actions are queued by their callers (HTTP server task, simulator link task)
// and executed on the main task by app_control_process(), preserving the
// project's single-writer rule for the panel and scene switcher.
typedef enum {
  APP_CTRL_NEXT_PRESET,
  APP_CTRL_PREV_PRESET,
  APP_CTRL_PRESET,       // arg: preset index (0..PRESET_COUNT-1)
  APP_CTRL_SCENE,        // str: scene name
  APP_CTRL_BRIGHTNESS,   // arg: 0..255 (persisted)
  APP_CTRL_REFRESH_WEATHER,
  APP_CTRL_REDRAW,
  APP_CTRL_FAKE_WEATHER, // str: clear|cloudy|rain|snow|invalid (simulator only)
  APP_CTRL_FAKE_TIME,    // str: "HH:MM", empty clears (simulator only)
} app_control_cmd_t;

// Create the control queue. Call once from the main task before any producer
// (HTTP server, simulator link) can queue actions.
esp_err_t app_control_init(void);

// Queue an action. Returns ESP_ERR_NOT_SUPPORTED for simulator-only actions on
// hardware builds, ESP_ERR_INVALID_ARG for bad arguments, ESP_ERR_NO_MEM when
// the queue is full.
esp_err_t app_control_request(app_control_cmd_t cmd, int arg, const char *str);

// Execute queued actions. Call from the main loop (main task).
void app_control_process(void);
