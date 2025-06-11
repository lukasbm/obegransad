#pragma once

#include "error.h"
#include <stdint.h>

#define BUTTON_PIN 20 // D7 (GPIO 20)

// right now the wifi manager uses 30% of flash storage alone, if we run out of space, use this instead: https://github.com/prampec/IotWebConf

void wifi_setup(void);
void wifi_clear_credentials(void);
bool wifi_check(void);
DeviceError enter_light_sleep(uint64_t seconds);
