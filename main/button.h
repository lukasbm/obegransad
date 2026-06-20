#pragma once

#include <esp_err.h>

// Minimal in-house button driver for the single user button (BUTTON_PIN).
//
// Replaces the espressif/button managed component: it configures the GPIO as a
// pulled-up input and starts a polling task that debounces and classifies
// presses, posting APP_EVT_BUTTON_SHORT / _LONG / _DOUBLE on the app event bus.
// It logs every debounced edge and decision so behaviour is observable on the
// monitor.
esp_err_t button_init();
