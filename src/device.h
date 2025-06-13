#pragma once

#include "error.h"
#include <stdint.h>

#define BUTTON_PIN 20 // D7 (GPIO 20)

// right now the wifi manager uses 30% of flash storage alone, if we run out of space, use this instead: https://github.com/prampec/IotWebConf

void wifi_setup(void);
void wifi_clear_credentials(void);
bool wifi_check(void);
bool wifi_is_portal_active();
DeviceError enter_light_sleep(uint64_t seconds);

void captive_portal_tick();
void captive_portal_start();
void captive_portal_stop();
void wifi_handle_timeout(); // Call in main loop to handle WiFi timeout non-blocking
