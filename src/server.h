#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include "device.h"

static const char *PORTAL_NAME = "Obegransad-Setup"; // captive portal name (SSID)
static const IPAddress PORTAL_IP(192, 168, 4, 1);    // captive portal IP address

class SettingsServer
{
private:
    AsyncWebServer server;

public:
    SettingsServer() : server(80) {}
    DeviceError start(void);
    void stop(void);
};
