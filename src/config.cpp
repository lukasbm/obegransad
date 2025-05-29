#include "config.h"

static AsyncWebServer server(80);

// define default config
Config::Config()
    : brightness_day(200),
      brightness_night(30),
      weather_latitude(49.4613909),
      weather_longitude(11.1540788),
      timezone("CET-1CEST,M3.5.0,M10.5.0/3"),
      anniversary_day(21),
      anniversary_month(5)
{
    // default off-times
    off_time_everyday.push_back({0, 0, 6, 0});     // everyday 0:00-6:00
    off_time_weekdays.push_back({22, 30, 23, 59}); // weekdays 22:30-23:59
    // weekends left empty
    off_time_weekends.clear();
}

Config settings;

bool Config::fromJson(const JsonObject &root)
{
    // primitives (fall back to existing values if missing)
    brightness_day = root["brightness_day"] | brightness_day;
    brightness_night = root["brightness_night"] | brightness_night;
    weather_latitude = root["weather_latitude"] | weather_latitude;
    weather_longitude = root["weather_longitude"] | weather_longitude;
    timezone = root["timezone"] | timezone;

    // helper lambda to read an array of OffTime
    auto replaceRead = [&](const char *key, std::vector<OffTime> &vec)
    {
        if (!root[key].is<JsonArray>())
        {
            return;
        }
        vec.clear();
        for (JsonObject item : root[key].as<JsonArray>())
        {
            OffTime ot;
            ot.from_hour = item["from_hour"] | 0;
            ot.from_minute = item["from_minute"] | 0;
            ot.to_hour = item["to_hour"] | 0;
            ot.to_minute = item["to_minute"] | 0;
            vec.push_back(ot);
        }
    };

    replaceRead("off_time_everyday", off_time_everyday);
    replaceRead("off_time_weekdays", off_time_weekdays);
    replaceRead("off_time_weekends", off_time_weekends);

    return true;
}

void Config::toJson(JsonObject &root) const
{
    // primitives
    root["brightness_day"] = brightness_day;
    root["brightness_night"] = brightness_night;
    root["weather_latitude"] = weather_latitude;
    root["weather_longitude"] = weather_longitude;
    root["timezone"] = timezone;

    // helper lambda to write an array
    auto writeOff = [&](const char *key, const std::vector<OffTime> &vec)
    {
        JsonArray arr = root[key].to<JsonArray>();
        for (auto &ot : vec)
        {
            JsonObject o = arr.add<JsonObject>();
            o["from_hour"] = ot.from_hour;
            o["from_minute"] = ot.from_minute;
            o["to_hour"] = ot.to_hour;
            o["to_minute"] = ot.to_minute;
        }
    };

    writeOff("off_time_everyday", off_time_everyday);
    writeOff("off_time_weekdays", off_time_weekdays);
    writeOff("off_time_weekends", off_time_weekends);
}

void replyConfig(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    settings.toJson(root);
    String payload;
    serializeJson(doc, payload);
    request->send(200, "application/to_json", payload);
}

void handleNewConfig(AsyncWebServerRequest *request)
{
    // get body
    if (!request->hasArg("plain"))
    {
        request->send(400, "text/plain", "Invalid request: missing JSON body");
        return;
    }
    String body = request->arg("plain");

    // deserialize body
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err)
    {
        request->send(400, "application/to_json",
                      String("{\"error\":\"Invalid JSON: ") + err.c_str() + "\"}");
        return;
    }

    // parse settings
    if (!settings.fromJson(doc.as<JsonObject>()))
    {
        request->send(400, "application/to_json",
                      "{\"error\":\"Invalid settings\"}");
        return;
    }

    // save
    request->send(200, "application/to_json", "{\"success\":\"Settings saved\"}");
}

void setup_config_server(void)
{
    server.on("/config", HTTP_GET, replyConfig);
    server.on("/config", HTTP_POST, handleNewConfig);
    server.begin();
}
