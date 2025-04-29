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
    float weather_update_interval; // in seconds

    int brightness_day;
    int brightness_night;

    std::vector<OffTime> off_time_everyday;
    std::vector<OffTime> off_time_weekdays;
    std::vector<OffTime> off_time_weekends;

    JsonObject json() const
    {
        JsonObject obj;

        // weather
        JsonObject weather = obj.createNestedObject("weather");
        weather["latitude"] = weather_latitude;
        weather["longitude"] = weather_longitude;
        weather["update_interval"] = weather_update_interval;

        // clock
        JsonObject clock = obj.createNestedObject("clock");
        clock["brightness_day"] = brightness_day;
        clock["brightness_night"] = brightness_night;
        JsonObject off_times = clock.createNestedObject("off_times");
        JsonArray off_time_weekdays_arr = clock.createNestedArray("off_time_weekdays");
        for (const OffTime &off_time : off_time_weekdays)
        {
            JsonObject off_time_obj = off_time_weekdays_arr.createNestedObject();
            off_time_obj["start_hour"] = off_time.start_hour;
            off_time_obj["start_minute"] = off_time.start_minute;
            off_time_obj["end_hour"] = off_time.end_hour;
            off_time_obj["end_minute"] = off_time.end_minute;
        }
        JsonArray off_time_weekends_arr = clock.createNestedArray("off_time_weekends");
        for (const OffTime &off_time : off_time_weekends)
        {
            JsonObject off_time_obj = off_time_weekends_arr.createNestedObject();
            off_time_obj["start_hour"] = off_time.start_hour;
            off_time_obj["start_minute"] = off_time.start_minute;
            off_time_obj["end_hour"] = off_time.end_hour;
            off_time_obj["end_minute"] = off_time.end_minute;
        }
        JsonArray off_time_everyday_arr = clock.createNestedArray("off_time_everyday");
        for (const OffTime &off_time : off_time_everyday)
        {
            JsonObject off_time_obj = off_time_everyday_arr.createNestedObject();
            off_time_obj["start_hour"] = off_time.start_hour;
            off_time_obj["start_minute"] = off_time.start_minute;
            off_time_obj["end_hour"] = off_time.end_hour;
            off_time_obj["end_minute"] = off_time.end_minute;
        }
        return obj;
    }
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
    request->send(200, "application/json", payload);
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
    server.on("/config", HTTP_POST, handleNewConfig);
    server.begin();
}
