#include "portal.h"
#include "device.h"
#include "config.h"

// portal

Portal::Portal() : server(), dns()
{
}

void Portal::start()
{
    // switch mode
    WiFi.mode(WIFI_AP); // switch to AP mode

    if (!WiFi.softAPsetHostname(PORTAL_NAME))
    {
        Serial.println("[PORTAL] Failed to set hostname for soft AP");
        WiFi.mode(WIFI_STA); // switch back to STA mode
        return;
    }

    // start the soft AP
    if (!WiFi.softAP(PORTAL_NAME, nullptr, 0, false, 2, false))
    {
        Serial.println("[PORTAL] Failed to start soft AP");
        WiFi.mode(WIFI_STA); // switch back to STA mode
        return;
    }
    Serial.println("[PORTAL] Soft AP started");

    // set the IP address of the soft AP
    if (!WiFi.softAPConfig(PORTAL_IP, PORTAL_IP, IPAddress(255, 255, 255, 0)))
    {
        Serial.println("[PORTAL] Failed to set soft AP config");
        WiFi.softAPdisconnect(true); // disconnect the soft AP
        WiFi.mode(WIFI_STA);         // switch back to STA mode
        return;
    }

    Serial.println("[PORTAL] Soft AP config set");

    // start DNS server
    if (!dns.start(53, "*", WiFi.softAPIP()))
    {
        Serial.println("[PORTAL] Failed to start DNS server");
        return;
    }
    
    // start the HTTP server
    server.start();

    Serial.println("[PORTAL] Server started");
}

void Portal::stop()
{
    // stop the server
    server.stop();

    dns.stop();

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    wifi_connect(settings.ssid, settings.password); // reconnect to the configured Wi-Fi

    Serial.println("[PORTAL] Server stopped");
}

void Portal::tick()
{
    // process DNS requests
    dns.processNextRequest();

    // No need to call server.handleClient() for AsyncWebServer
}

/// Server

// handler functions

static void handle_get_networks(AsyncWebServerRequest *request)
{
    String out;
    JsonDocument doc;
    JsonArray networksArray = doc.to<JsonArray>();
    // get wifi networks from last scan
    std::vector<NetworkInfo> networks = wifi_nearby_networks();
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

static void offtime_helper(JsonDocument &obj, const OffTime &offtime, const String &prefix)
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
}

static void handle_get_settings(AsyncWebServerRequest *request)
{
    String out;
    JsonDocument doc;
    // same names as in Settings struct and preferences (NVS)

    doc["wifi_ssid"] = settings.ssid;
    doc["wifi_password"] = settings.password;
    doc["brightness_day"] = settings.brightness_day;
    doc["brightness_night"] = settings.brightness_night;
    // off times 1
    offtime_helper(doc, settings.offtime1, "offtime1_");
    // off times 2
    offtime_helper(doc, settings.offtime2, "offtime2_");
    // off times 3
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
    Preferences p;
    p.begin("app", false); // RW
    p.clear();
    p.end();
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
    // off times 1
    settings.offtime1.from_hour = doc["offtime1_from_hour"] | settings.offtime1.from_hour;
    settings.offtime1.from_minute = doc["offtime1_from_minute"] | settings.offtime1.from_minute;
    settings.offtime1.to_hour = doc["offtime1_to_hour"] | settings.offtime1.to_hour;
    settings.offtime1.to_minute = doc["offtime1_to_minute"] | settings.offtime1.to_minute;
    settings.offtime1.sunday = doc["offtime1_sunday"] | settings.offtime1.sunday;
    settings.offtime1.monday = doc["offtime1_monday"] | settings.offtime1.monday;
    settings.offtime1.tuesday = doc["offtime1_tuesday"] | settings.offtime1.tuesday;
    settings.offtime1.wednesday = doc["offtime1_wednesday"] | settings.offtime1.wednesday;
    settings.offtime1.thursday = doc["offtime1_thursday"] | settings.offtime1.thursday;
    settings.offtime1.friday = doc["offtime1_friday"] | settings.offtime1.friday;
    settings.offtime1.saturday = doc["offtime1_saturday"] | settings.offtime1.saturday;
    // off times 2
    settings.offtime2.from_hour = doc["offtime2_from_hour"] | settings.offtime2.from_hour;
    settings.offtime2.from_minute = doc["offtime2_from_minute"] | settings.offtime2.from_minute;
    settings.offtime2.to_hour = doc["offtime2_to_hour"] | settings.offtime2.to_hour;
    settings.offtime2.to_minute = doc["offtime2_to_minute"] | settings.offtime2.to_minute;
    settings.offtime2.sunday = doc["offtime2_sunday"] | settings.offtime2.sunday;
    settings.offtime2.monday = doc["offtime2_monday"] | settings.offtime2.monday;
    settings.offtime2.tuesday = doc["offtime2_tuesday"] | settings.offtime2.tuesday;
    settings.offtime2.wednesday = doc["offtime2_wednesday"] | settings.offtime2.wednesday;
    settings.offtime2.thursday = doc["offtime2_thursday"] | settings.offtime2.thursday;
    settings.offtime2.friday = doc["offtime2_friday"] | settings.offtime2.friday;
    settings.offtime2.saturday = doc["offtime2_saturday"] | settings.offtime2.saturday;
    // off times 3
    settings.offtime3.from_hour = doc["offtime3_from_hour"] | settings.offtime3.from_hour;
    settings.offtime3.from_minute = doc["offtime3_from_minute"] | settings.offtime3.from_minute;
    settings.offtime3.to_hour = doc["offtime3_to_hour"] | settings.offtime3.to_hour;
    settings.offtime3.to_minute = doc["offtime3_to_minute"] | settings.offtime3.to_minute;
    settings.offtime3.sunday = doc["offtime3_sunday"] | settings.offtime3.sunday;
    settings.offtime3.monday = doc["offtime3_monday"] | settings.offtime3.monday;
    settings.offtime3.tuesday = doc["offtime3_tuesday"] | settings.offtime3.tuesday;
    settings.offtime3.wednesday = doc["offtime3_wednesday"] | settings.offtime3.wednesday;
    settings.offtime3.thursday = doc["offtime3_thursday"] | settings.offtime3.thursday;
    settings.offtime3.friday = doc["offtime3_friday"] | settings.offtime3.friday;
    settings.offtime3.saturday = doc["offtime3_saturday"] | settings.offtime3.saturday;

    // save settings to persistent storage
    write_to_persistent_storage(settings);

    // respond
    req->send(200, "application/json", R"({"ok":true})");
}

void SettingsServer::start()
{
    // initialize LittleFS
    if (!LittleFS.begin())
    {
        Serial.println("[PORTAL] Failed to mount LittleFS");
        return;
    }

    // handle root page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });

    // handle static files
    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html");

    // captive portal URLs
    server.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request) { // Android
        request->redirect("/");
    });
    server.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *request) { // iOS
        request->send(200, "text/html", "<!doctype html>");
    });

    // handle API requests
    // server.on("/api/settings", HTTP_GET, handle_get_settings);
    // server.on("/api/networks", HTTP_GET, handle_get_networks);
    // server.on("/api/settings", HTTP_DELETE, handle_delete_settings);
    // server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, handle_post_settings);

    // start the server
    server.begin();

    Serial.println("[PORTAL] Server started");
}

void SettingsServer::stop()
{
    server.end(); // stop the HTTP server
    LittleFS.end();
}
