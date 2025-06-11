#include "server.h"
#include "config.h"

static void handle_get_settings(AsyncWebServerRequest *request)
{
    String out;
    JsonDocument doc;

    // turn off_hours into an array of booleans
    JsonArray off_hours_array = doc["off_hours"].to<JsonArray>();
    for (int i = 0; i < 24; ++i)
    {
        off_hours_array.add((gSettings.off_hours & (1 << i)) != 0); // true if the bit is set
    }

    // same names as in Settings struct and preferences (NVS)
    doc["brightness_day"] = gSettings.brightness_day;
    doc["brightness_night"] = gSettings.brightness_night;
    doc["off_hours"] = off_hours_array; // use the array we just created
    doc["weather_latitude"] = gSettings.weather_latitude;
    doc["weather_longitude"] = gSettings.weather_longitude;
    doc["timezone"] = gSettings.timezone;
    doc["anniversary_day"] = gSettings.anniversary_day;
    doc["anniversary_month"] = gSettings.anniversary_month;

    // serialize the document to JSON
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

static void handle_delete_settings(AsyncWebServerRequest *request)
{
    // delete all settings
    clear_persistent_storage();
    request->send(200, "application/json", R"({"ok":true})");
    // restart the device to apply changes
    ESP.restart();
}

// chunked handler for settings update
void handle_put_settings(AsyncWebServerRequest *req,
                         uint8_t *data, size_t len,
                         size_t index, size_t total)
{
    // chunking
    static String body;
    if (index == 0)
        body = ""; // first chunk
    body += String((char *)data, len);
    if (index + len != total)
        return;
    // done, process regularly

    JsonDocument doc;
    if (deserializeJson(doc, body))
    {
        req->send(400, "application/json", R"({"error":"bad json"})");
        return;
    }

    auto off_hours_helper = [doc]() -> uint32_t
    {
        if (doc["off_hours"].isNull())
        return gSettings.off_hours; // no off hours set
        // off_hours is an array of booleans
        uint32_t off_hours = 0;
        for (int i = 0; i < 24; ++i)
        {
            if (doc["off_hours"][i].is<bool>() && doc["off_hours"][i].as<bool>())
            {
                off_hours |= (1 << i); // set the bit for this hour
            }
        }
        return off_hours;
    };

    // parse settings
    Settings parsed{
        .brightness_day = uint8_t(doc["brightness_day"] | gSettings.brightness_day),
        .brightness_night = uint8_t(doc["brightness_night"] | gSettings.brightness_night),
        .off_hours = off_hours_helper(),
        .weather_latitude = double(doc["weather_latitude"] | gSettings.weather_latitude),
        .weather_longitude = double(doc["weather_longitude"] | gSettings.weather_longitude),
        .timezone = String(doc["timezone"] | gSettings.timezone),
        .anniversary_day = uint8_t(doc["anniversary_day"] | gSettings.anniversary_day),
        .anniversary_month = uint8_t(doc["anniversary_month"] | gSettings.anniversary_month)};

    if (!parsed.valid())
    {
        req->send(400, "application/json", R"({"error":"invalid settings"})");
        return;
    }

    // save settings to persistent storage
    write_to_persistent_storage(parsed);

    // update global settings
    gSettings = parsed;

    // respond
    req->send(200, "application/json", R"({"ok":true})");
}

DeviceError SettingsServer::start()
{
    // protect against multiple start
    static bool started = false;
    if (started)
    {
        Serial.println("[PORTAL] Server already started");
        return ERR_NONE; // already started
    }
    started = true;
    Serial.println("[PORTAL] Starting server...");

    // initialize LittleFS
    if (!LittleFS.begin())
    {
        Serial.println("[PORTAL] Failed to mount LittleFS");
        // try reformat
        if (!LittleFS.format())
        {
            Serial.println("[PORTAL] Failed to format LittleFS");
            return ERR_LITTLE_FS;
        }
        // try to mount again
        if (!LittleFS.begin())
        {
            Serial.println("[PORTAL] Failed to mount LittleFS after formatting");
            return ERR_LITTLE_FS;
        }
    }

    // captive portal URLs
    server.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request) { // Android
        request->redirect("/");                                               // redirect to portal
    });
    server.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *request) { // iOS
        request->send(200, "text/html", "<!doctype html>");
    });
    server.on("/static/hotspot.txt", HTTP_ANY, [](AsyncWebServerRequest *request) { // Samsung, Fedora
        request->redirect("/");                                                     // redirect to portal
    });

    // handle API requests
    server.on("/api/settings", HTTP_GET, handle_get_settings);
    server.on("/api/settings", HTTP_DELETE, handle_delete_settings);
    server.on("/api/settings", HTTP_PUT, [](AsyncWebServerRequest *req) {}, nullptr, handle_put_settings);

    // serve everything statically from LittleFS
    // empty path means portal
    // needs to be last or it will catch everything
    server.serveStatic("/", LittleFS, "/").setDefaultFile("portal.html"); // .setCacheControl("max-age=86400");

    // start the server
    server.begin();

    Serial.println("[PORTAL] Server started");

    return ERR_NONE;
}

void SettingsServer::stop()
{
    server.end();
    LittleFS.end();
}
