// Web config interface

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);
Settings settings;
JsonDocument settings_doc; // last sucessfully received settings (for reply)
// this is fine as the post endpoint is the only source that modifies the settings

struct OffTime
{
    int start_hour;
    int start_minute;
    int end_hour;
    int end_minute;
};

struct Settings
{
    float weather_latitude;
    float weather_longitude;
    float weather_update_interval; // in minutes

    std::vector<OffTime> off_time_everyday;
    std::vector<OffTime> off_time_weekdays;
    std::vector<OffTime> off_time_weekends;
};

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

bool parseSettings(Settings &res, const JsonDocument &doc)
{
    if (!doc.containsKey("weather_latitude") ||
        !doc.containsKey("weather_longitude"))
        return false;

    if (doc.containsKey("off_time_everyday"))
        parseOffTimes(res.off_time_everyday, doc["off_time_everyday"]);
    if (doc.containsKey("off_time_weekdays"))
        parseOffTimes(res.off_time_weekdays, doc["off_time_weekdays"]);
    if (doc.containsKey("off_time_weekends"))
        parseOffTimes(res.off_time_weekends, doc["off_time_weekends"]);

    res.weather_latitude = doc["weather"]["latitude"];
    res.weather_longitude = doc["weather"]["longitude"];
    res.weather_update_interval = doc["weather_update_interval"] | 5; // default to 5 if not set

    return true;
}

void replyConfig(AsyncWebServerRequest *request)
{
    String payload;
    serializeJson(settings_doc, payload);
    request->send(200, "application/json", payload);
}

void receiveConfig(AsyncWebServerRequest *request)
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
        request->send(400, "application/json",
                      String("{\"error\":\"Invalid JSON: ") +
                          err.c_str() + "\"}");

        return;
    }

    // parse settings
    Settings parsed;
    if (!parseSettings(parsed, doc))
    {
        request->send(400, "application/json",
                      "{\"error\":\"Invalid settings\"}");
        return;
    }

    // save
    settings = parsed;
    request->send(200, "application/json", "{\"success\":\"Settings saved\"}");
}

void setup_config_server(void)
{
    server.on("/config", HTTP_GET, replyConfig);
    server.on("/config", HTTP_POST, receiveConfig);
    server.begin();
}
