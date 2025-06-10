#include "server.h"
#include "device.h"
#include "config.h"

static void handle_get_networks(AsyncWebServerRequest *request)
{
    String out;
    JsonDocument doc;
    JsonArray networksArray = doc.to<JsonArray>();
    // get wifi networks from last scan
    std::vector<NetworkInfo> networks = wifi_nearby_networks(); // FIXME: use instance var of portal
    for (const auto &network : networks)
    {
        JsonObject net = networksArray.add<JsonObject>();
        net["ssid"] = network.ssid;
        net["rssi"] = network.rssi;
        net["encryptionType"] = network.encryptionType;
        net["channel"] = network.channel;
        net["quality"] = network.quality;
    }
    // serialize the array to JSON
    serializeJson(networksArray, out);
    request->send(200, "application/json", out);
}

static void handle_get_settings(AsyncWebServerRequest *request)
{
    String out;
    JsonDocument doc;
    // same names as in Settings struct and preferences (NVS)

    auto offtime_helper = [](JsonDocument &obj, const OffTime &offtime, const String &prefix)
    {
        obj[prefix + "from_hour"] = offtime.from_hour;
        obj[prefix + "from_minute"] = offtime.from_minute;
        obj[prefix + "to_hour"] = offtime.to_hour;
        obj[prefix + "to_minute"] = offtime.to_minute;
        // spread bitmask
        obj[prefix + "sunday"] = bool(offtime.sunday);
        obj[prefix + "monday"] = bool(offtime.monday);
        obj[prefix + "tuesday"] = bool(offtime.tuesday);
        obj[prefix + "wednesday"] = bool(offtime.wednesday);
        obj[prefix + "thursday"] = bool(offtime.thursday);
        obj[prefix + "friday"] = bool(offtime.friday);
        obj[prefix + "saturday"] = bool(offtime.saturday);
    };

    doc["wifi_ssid"] = settings.ssid;
    doc["wifi_password"] = settings.password;
    doc["brightness_day"] = settings.brightness_day;
    doc["brightness_night"] = settings.brightness_night;
    offtime_helper(doc, settings.offtime1, "offtime1_");
    offtime_helper(doc, settings.offtime2, "offtime2_");
    offtime_helper(doc, settings.offtime3, "offtime3_");
    doc["weather_latitude"] = settings.weather_latitude;
    doc["weather_longitude"] = settings.weather_longitude;
    doc["timezone"] = settings.timezone;
    doc["anniversary_day"] = settings.anniversary_day;
    doc["anniversary_month"] = settings.anniversary_month;
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

    // TODO: validate required fields (e.g. types via obj[key].is<Type>())

    auto offtime_helper = [](JsonDocument &obj, OffTime &offtime, const String &prefix)
    {
        // obj[prefix + "from_hour"] = offtime.from_hour;
        offtime.from_hour = obj[prefix + "_from_hour"] | offtime.from_hour;
        offtime.from_minute = obj[prefix + "_from_minute"] | offtime.from_minute;
        offtime.to_hour = obj[prefix + "_to_hour"] | offtime.to_hour;
        offtime.to_minute = obj[prefix + "_to_minute"] | offtime.to_minute;
        offtime.sunday = obj[prefix + "_sunday"] | offtime.sunday;
        offtime.monday = obj[prefix + "_monday"] | offtime.monday;
        offtime.tuesday = obj[prefix + "_tuesday"] | offtime.tuesday;
        offtime.wednesday = obj[prefix + "_wednesday"] | offtime.wednesday;
        offtime.thursday = obj[prefix + "_thursday"] | offtime.thursday;
        offtime.friday = obj[prefix + "_friday"] | offtime.friday;
        offtime.saturday = obj[prefix + "_saturday"] | offtime.saturday;
        return offtime;
    };

    // merge only what’s present
    settings.brightness_day = doc["brightness_day"] | settings.brightness_day;
    settings.brightness_night = doc["brightness_night"] | settings.brightness_night;
    settings.ssid = doc["wifi_ssid"] | settings.ssid;
    settings.password = doc["wifi_password"] | settings.password;
    settings.weather_latitude = doc["weather_latitude"] | settings.weather_latitude;
    settings.weather_longitude = doc["weather_longitude"] | settings.weather_longitude;
    settings.timezone = doc["timezone"] | settings.timezone;
    settings.anniversary_day = doc["anniversary_day"] | settings.anniversary_day;
    settings.anniversary_month = doc["anniversary_month"] | settings.anniversary_month;
    settings.offtime1 = offtime_helper(doc, settings.offtime1, "offtime1");
    settings.offtime2 = offtime_helper(doc, settings.offtime2, "offtime2");
    settings.offtime3 = offtime_helper(doc, settings.offtime3, "offtime3");

    // save settings to persistent storage
    write_to_persistent_storage(settings);

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
    server.on("/api/networks", HTTP_GET, handle_get_networks);
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
