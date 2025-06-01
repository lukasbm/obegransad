#pragma once

#include <Arduino.h>

// if from is like 22:00 and to is like 06:00, it will run until the next day, but it needs to be part of "days"
struct OffTime
{
    uint8_t from_hour;
    uint8_t from_minute;
    uint8_t to_hour;
    uint8_t to_minute;
    union
    {
        uint8_t days; // LSB is Sunday, LSB<<1 is Monday, LSB<<2 is Tuesday, etc.
        struct
        {
            uint8_t sunday : 1;
            uint8_t monday : 1;
            uint8_t tuesday : 1;
            uint8_t wednesday : 1;
            uint8_t thursday : 1;
            uint8_t friday : 1;
            uint8_t saturday : 1;
            uint8_t reserved : 1; // reserved for future use
        };
    };

    // isInside is true if the time is inside the off time range
    bool isInside(const struct tm &time) const;
};

// trying to use the names consistent between this struct, json and preferences (NVS)
struct Settings
{
    bool initial_setup_done; // true if the initial setup has been done, false otherwise. If false, then the captive portal open on every boot.
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
};

void write_to_persistent_storage(Settings &settings);
void read_from_persistent_storage(const Settings *settings);

extern Settings settings; // global settings structure
