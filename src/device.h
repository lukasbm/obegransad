#pragma once

#include <WiFi.h>
#include <esp_sleep.h>
#include <string.h>
#include <Arduino.h>
#include <WiFiManager.h>
#include <stdint.h>

#define BUTTON_PIN 20 // D7 (GPIO 20)

enum DeviceError
{
    ERR_NONE = 0,
    ERR_WIFI,
    ERR_SLEEP,
};

extern WiFiManager wifiManager;

void wifi_setup(void);
void wifi_clear_credentials(void);
bool wifi_check(void);
void wifi_start_captive_portal(void);
void wifi_stop_captive_portal(void);

DeviceError enter_light_sleep(uint64_t seconds);

void display_wifi_setup_prompt(void);
void display_device_error(DeviceError err);
