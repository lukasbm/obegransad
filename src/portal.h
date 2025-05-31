#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

static const char *PORTAL_NAME = "Obegransad-Setup"; // captive portal name (SSID)

class SettingsServer
{
private:
    AsyncWebServer server;

public:
    SettingsServer() : server(80) {}
    void start(void);
    void stop(void);
};

class Portal
{
public:
    Portal();

private:
    SettingsServer server; // HTTP server for captive portal
    DNSServer dns;         // DNS server for captive portal

    void start(void);
    void stop(void);
    void tick(void);
};
