#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

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
};

// TODO: draw a state machine about of wifi and so on
// https://github.com/copilot/c/39104fee-961b-44c4-adfb-4286e53e51cb
