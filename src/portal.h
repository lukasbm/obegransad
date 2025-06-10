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

class Portal
{
public:
    Portal();

    void start(void);
    void stop(void);
    void tick(void);
    bool isOpen(void) const;

private:
    bool open = false;     // true if the portal is open, false otherwise
    SettingsServer server; // HTTP server for captive portal
    DNSServer dns;         // DNS server for captive portal

    std::vector<NetworkInfo> networks;
};
