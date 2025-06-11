#include "server.h"
#include "config.h"

static void handle_get_settings(AsyncWebServerRequest *request)
{
    String out;
    JsonDocument doc;
    // same names as in Settings struct and preferences (NVS)

    doc["brightness_day"] = gSettings.brightness_day;
    doc["brightness_night"] = gSettings.brightness_night;
    // offtime_helper(doc, gSettings.offtime3, "offtime3_");
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

// chunked POST handler for settings
void handle_post_settings(AsyncWebServerRequest *req,
                          uint8_t *data, size_t len,
                          size_t index, size_t total)
{
    static String body;
    if (index == 0)
        body = ""; // first chunk
    body += String((char *)data, len);

    if (index + len != total)
        return;

    // last chunk now
    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok)
    {
        req->send(400, "application/json", R"({"error":"bad json"})");
    }

    // TODO: validate settings
    if (!doc.is<JsonObject>())
    {
        req->send(400, "application/json", R"({"error":"not an object"})");
        return;
    }
    JsonObject obj = doc.as<JsonObject>();

    // parse .... = valid

    // merge only what’s present
    gSettings.brightness_day = doc["brightness_day"] | gSettings.brightness_day;
    gSettings.brightness_night = doc["brightness_night"] | gSettings.brightness_night;
    gSettings.weather_latitude = doc["weather_latitude"] | gSettings.weather_latitude;
    gSettings.weather_longitude = doc["weather_longitude"] | gSettings.weather_longitude;
    gSettings.timezone = doc["timezone"] | gSettings.timezone;
    gSettings.anniversary_day = doc["anniversary_day"] | gSettings.anniversary_day;
    gSettings.anniversary_month = doc["anniversary_month"] | gSettings.anniversary_month;
    // gSettings.offtime1 = offtime_helper(doc, gSettings.offtime1, "offtime1");

    // save settings to persistent storage
    write_to_persistent_storage(gSettings);

    // respond
    req->send(200, "application/json", R"({"ok":true})");
}

DeviceError SettingsServer::start()
{
    // FIXME: protect against multiple starts
    // if (server. isRunning())
    // {
    //     Serial.println("[PORTAL] Server already running");
    //     return ERR_NONE;
    // }
    // Serial.println("[PORTAL] Starting settings server...");

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
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, handle_post_settings);

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
