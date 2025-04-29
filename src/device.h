#pragma once

#include <WiFi.h>
#include <esp_sleep.h>

#define WAKEUP_PIN GPIO_NUM_9 // button pin!

// TODO: Add WiFi credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

void enter_light_sleep(uint64_t seconds);
void wifi_connect(void);
void wifi_off();
