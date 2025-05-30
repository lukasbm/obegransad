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

struct NetworkInfo
{
    String ssid;
    int32_t rssi;
    wifi_auth_mode_t encryptionType;
    uint8_t channel;
    uint8_t quality; // 0-100
};

void wifi_setup(void);
void wifi_clear_credentials(void);
bool wifi_check(void);
std::vector<NetworkInfo> wifi_nearby_networks(void);
uint8_t wifi_rssi_quality(int rssi);

void wifi_connect(const String &ssid, const String &password);
void wifi_disconnect();
void wifi_reconnect();

DeviceError enter_light_sleep(uint64_t seconds);

// displays a wifi symbol on the panel to indicate the device is in AP mode.
void display_wifi_symbol(void);
void display_device_error(DeviceError err);
