#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ArduinoJson.h>

struct OffTime
{
    int start_hour;
    int start_minute;
    int end_hour;
    int end_minute;

    JsonObject to_json() const
    {
        JsonObject obj;
        obj["start_hour"] = start_hour;
        obj["start_minute"] = start_minute;
        obj["end_hour"] = end_hour;
        obj["end_minute"] = end_minute;
        return obj;
    }
};

struct Settings
{
    float weather_latitude;
    float weather_longitude;
    float weather_update_interval; // in seconds

    int brightness_day;
    int brightness_night;

    std::vector<OffTime> off_time_everyday;
    std::vector<OffTime> off_time_weekdays;
    std::vector<OffTime> off_time_weekends;

    JsonObject to_json() const
    {
        JsonObject obj;

        JsonObject weather = obj.createNestedObject("weather");
        weather["latitude"] = weather_latitude;
        weather["longitude"] = weather_longitude;
        weather["update_interval"] = weather_update_interval;

        JsonObject clock = obj.createNestedObject("clock");
        clock["brightness_day"] = brightness_day;
        clock["brightness_night"] = brightness_night;

        JsonArray off_time_weekdays_arr = clock.createNestedArray("off_time_weekdays");
        for (const OffTime &off_time : off_time_weekdays)
            off_time_weekdays_arr.add(off_time.to_json());

        JsonArray off_time_weekends_arr = clock.createNestedArray("off_time_weekends");
        for (const OffTime &off_time : off_time_weekends)
            off_time_weekends_arr.add(off_time.to_json());

        JsonArray off_time_everyday_arr = clock.createNestedArray("off_time_everyday");
        for (const OffTime &off_time : off_time_everyday)
            off_time_everyday_arr.add(off_time.to_json());

        return obj;
    }
};

static AsyncWebServer server(80);
struct Settings settings;
JsonDocument settings_doc; // last sucessfully received settings (for reply)
// this is fine as the post endpoint is the only source that modifies the settings

void parseOffTimes(std::vector<OffTime> &res, const JsonArray &doc);
void replyConfig(AsyncWebServerRequest *request);
void handleNewConfig(AsyncWebServerRequest *request);
void setup_config_server();
