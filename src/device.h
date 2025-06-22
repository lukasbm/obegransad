#pragma once

#include "error.h"
#include <stdint.h>

#define BUTTON_PIN 20 // D7 (GPIO 20)

// right now the wifi manager uses 30% of flash storage alone, if we run out of space, use this instead: https://github.com/prampec/IotWebConf

void wifi_clear_credentials(void);
bool wifi_check(void);
bool captive_portal_active();
bool wifi_setup(void);

DeviceError enter_light_sleep(uint64_t seconds);

void captive_portal_setup();
void captive_portal_tick();
void captive_portal_start();
void captive_portal_stop();
