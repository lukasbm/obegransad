#pragma once

#include <Arduino.h>


void portal_start(void);
void portal_stop(void);

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
