#pragma once

#include <WiFi.h>
#include <esp_sleep.h>
#include <string.h>
#include <Arduino.h>
#include <WiFiManager.h>

#define BUTTON_PIN 20 // D7 (GPIO 20)

void setup_device(void);

void enter_light_sleep(uint64_t seconds);

// DEPRECATED
void wifi_connect(void);

void wifi_off(void);

void wifi_clear_credentials(void);
