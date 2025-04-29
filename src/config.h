#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

struct OffTime
{
    uint8_t from_hour;
    uint8_t from_minute;
    uint8_t to_hour;
    uint8_t to_minute;
};

struct Config
{
    // your settings
    uint16_t brightness_day;
    uint16_t brightness_night;

    std::vector<OffTime> off_time_everyday;
    std::vector<OffTime> off_time_weekdays;
    std::vector<OffTime> off_time_weekends;

    double weather_latitude;
    double weather_longitude;
    uint32_t weather_update_interval;

    // 1) default ctor: builds your “fallback” config
    Config();

    // 2) read JSON into *this* (preserving defaults when keys missing)
    bool fromJson(const JsonObject &root);

    // 3) write *this* into a JsonObject
    void toJson(JsonObject &root) const;
};

void parseOffTimes(std::vector<OffTime> &res, const JsonArray &doc);
void replyConfig(AsyncWebServerRequest *request);
void handleNewConfig(AsyncWebServerRequest *request);
void setup_config_server();

// global variable
struct Config settings;
