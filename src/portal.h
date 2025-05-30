#pragma once

#include <Arduino.h>

struct OffTime
{
    uint8_t from_hour;
    uint8_t from_minute;
    uint8_t to_hour;
    uint8_t to_minute;
    uint8_t days; // Bitmask for days of the week (LSB = Monday, MSB (bit 7) = Sunday)
};

// trying to use the names consistent between this struct, json and preferences (NVS)
struct Settings
{
    String ssid;
    String password;
    uint8_t brightness_day;
    uint8_t brightness_night;
    OffTime offtime1;
    OffTime offtime2;
    OffTime offtime3;
    double weather_latitude;
    double weather_longitude;
    String timezone;
    uint8_t anniversary_day;
    uint8_t anniversary_month;

    Settings();

    void to_json(JsonDocument &doc);
    void from_json(const JsonDocument &doc);

    void save() { write_to_persistent_storage(*this); }
    void load() { read_from_persistent_storage(this); }
};

void write_to_persistent_storage(Settings &settings);
void read_from_persistent_storage(const Settings *settings);

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
