#include "config.h"

// Web config interface


void parseOffTimes(std::vector<OffTime> &res, const JsonArray &doc)
{
    res.clear();
    for (JsonObject item : doc)
    {
        OffTime off_time;
        off_time.start_hour = item["start_hour"] | 0;
        off_time.start_minute = item["start_minute"] | 0;
        off_time.end_hour = item["end_hour"] | 0;
        off_time.end_minute = item["end_minute"] | 0;
        // validate
        if (off_time.start_hour < 0 || off_time.start_hour > 23 ||
            off_time.start_minute < 0 || off_time.start_minute > 59 ||
            off_time.end_hour < 0 || off_time.end_hour > 23 ||
            off_time.end_minute < 0 || off_time.end_minute > 59)
        {
            Serial.println("Invalid time format");
            continue;
        }
        res.push_back(off_time);
    }
}

bool parseSettings(struct Settings &res, const JsonDocument &doc)
{
    // clock scene settings
    if (!doc.containsKey("clock"))
        return false;
    JsonObject clock = doc["clock"];
    if (clock.containsKey("off_time_everyday"))
        parseOffTimes(res.off_time_everyday, clock["off_time_everyday"]);
    if (clock.containsKey("off_time_weekdays"))
        parseOffTimes(res.off_time_weekdays, clock["off_time_weekdays"]);
    if (clock.containsKey("off_time_weekends"))
        parseOffTimes(res.off_time_weekends, clock["off_time_weekends"]);
    res.brightness_day = clock["brightness_day"] | 100;
    res.brightness_night = clock["brightness_night"] | 50;

    // weather settings
    if (!doc.containsKey("weather"))
        return false;
    JsonObject weather = doc["weather"];
    res.weather_latitude = weather["latitude"];
    res.weather_longitude = weather["longitude"];
    res.weather_update_interval = weather["update_interval"] | 300;

    return true;
}

void replyConfig(AsyncWebServerRequest *request)
{
    String payload;
    serializeJson(settings_doc, payload);
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
                      String("{\"error\":\"Invalid JSON: ") +
                          err.c_str() + "\"}");

        return;
    }

    // parse settings
    Settings parsed;
    if (!parseSettings(parsed, doc))
    {
        request->send(400, "application/to_json",
                      "{\"error\":\"Invalid settings\"}");
        return;
    }

    // save
    settings = parsed;
    request->send(200, "application/to_json", "{\"success\":\"Settings saved\"}");
}

void setup_config_server(void)
{
    server.on("/config", HTTP_GET, replyConfig);
    server.on("/config", HTTP_POST, handleNewConfig);
    server.begin();
}

// FIXME: i need a default settings object in case the user does not provide one
