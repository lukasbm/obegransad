#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <vector>

struct OffTime
{
    uint8_t from_hour;
    uint8_t from_minute;
    uint8_t to_hour;
    uint8_t to_minute;
};

struct Config
{
    uint8_t brightness_day;
    uint8_t brightness_night;
    std::vector<OffTime> off_time_everyday;
    std::vector<OffTime> off_time_weekdays;
    std::vector<OffTime> off_time_weekends;
    double weather_latitude;
    double weather_longitude;
    String timezone;
    uint8_t anniversary_day;
    uint8_t anniversary_month;

    Config();
    bool fromJson(const JsonObject &root);
    void toJson(JsonObject &root) const;
};

void parseOffTimes(std::vector<OffTime> &res, const JsonArray &doc);
void replyConfig(AsyncWebServerRequest *request);
void handleNewConfig(AsyncWebServerRequest *request);
void setup_config_server();

extern Config settings;
