#pragma once

#include "esp_err.h"

// HTTP config server: exposes the device state, configuration, control actions
// and the event log. See docs/openapi.yaml for the API contract.
//
// Start after config_store_init()/config_store_apply() and after the scene
// switcher/state machine are initialized. Control actions are queued for the
// main task; the server never touches the panel itself.
esp_err_t config_server_start(void);
