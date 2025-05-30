#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstdint>
#include <Preferences.h>
#include <DNSServer.h>
#include <WiFi.h>

struct OffTime
{
    uint8_t from_hour;
    uint8_t from_minute;
    uint8_t to_hour;
    uint8_t to_minute;
    uint8_t days; // Bitmask for days of the week (LSB = Monday, MSB (bit 7) = Sunday)
};

struct Settings
{
    int8_t brightness_day;
    uint8_t brightness_night;
    OffTime off_time_1;
    OffTime off_time_2;
    OffTime off_time_3;
    double weather_latitude;
    double weather_longitude;
    String timezone;
    uint8_t anniversary_day;
    uint8_t anniversary_month;
} cfg;

class Portal
{
};

