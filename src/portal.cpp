#include "portal.h"
#include "device.h"

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstdint>
#include <Preferences.h>
#include <DNSServer.h>
#include <WiFi.h>

static Settings cfg; // global settings structure

// implicitly sets default values.
void read_from_persistent_storage(Settings &settings)
{
    Preferences p;
    p.begin("app", true); // read-only mode
    // wifi
    settings.ssid = p.getString("wifi_ssid", "");
    settings.password = p.getString("wifi_password", "");
    // panel
    settings.brightness_day = p.getUChar("brightness_day", 200);
    settings.brightness_night = p.getUChar("brightness_night", 50);
    // offtime 1
    settings.offtime1.from_hour = p.getUChar("offtime1_from_hour", 0);
    settings.offtime1.from_minute = p.getUChar("offtime1_from_minute", 0);
    settings.offtime1.to_hour = p.getUChar("offtime1_to_hour", 0);
    settings.offtime1.to_minute = p.getUChar("offtime1_to_minute", 0);
    settings.offtime1.days = p.getUChar("offtime1_days", 0b0); // no days
    // offtime 2
    settings.offtime2.from_hour = p.getUChar("offtime2_from_hour", 0);
    settings.offtime2.from_minute = p.getUChar("offtime2_from_minute", 0);
    settings.offtime2.to_hour = p.getUChar("offtime2_to_hour", 0);
    settings.offtime2.to_minute = p.getUChar("offtime2_to_minute", 0);
    settings.offtime2.days = p.getUChar("offtime2_days", 0b0); // no days
    // offtime 3
    settings.offtime3.from_hour = p.getUChar("offtime3_from_hour", 0);
    settings.offtime3.from_minute = p.getUChar("offtime3_from_minute", 0);
    settings.offtime3.to_hour = p.getUChar("offtime3_to_hour", 0);
    settings.offtime3.to_minute = p.getUChar("offtime3_to_minute", 0);
    settings.offtime3.days = p.getUChar("offtime3_days", 0b0); // no days
    // weather
    settings.weather_latitude = p.getDouble("weather_latitude", 0.0);   // equator
    settings.weather_longitude = p.getDouble("weather_longitude", 0.0); // prime meridian
    // timezone
    settings.timezone = p.getString("timezone", "UTC");
    // anniversary
    settings.anniversary_day = p.getUChar("anniversary_day", 1);
    settings.anniversary_month = p.getUChar("anniversary_month", 1);

    p.end();
}

void write_to_persistent_storage(Settings &settings)
{
    Preferences p;
    p.begin("app", false); // read-write mode
    // wifi
    p.putString("wifi_ssid", settings.ssid);
    p.putString("wifi_password", settings.password);
    // panel
    p.putUChar("brightness_day", settings.brightness_day);
    p.putUChar("brightness_night", settings.brightness_night);
    // offtime 1
    p.putUChar("offtime1_from_hour", settings.offtime1.from_hour);
    p.putUChar("offtime1_from_minute", settings.offtime1.from_minute);
    p.putUChar("offtime1_to_hour", settings.offtime1.to_hour);
    p.putUChar("offtime1_to_minute", settings.offtime1.to_minute);
    p.putUChar("offtime1_days", settings.offtime1.days);
    // offtime 2
    p.putUChar("offtime2_from_hour", settings.offtime2.from_hour);
    p.putUChar("offtime2_from_minute", settings.offtime2.from_minute);
    p.putUChar("offtime2_to_hour", settings.offtime2.to_hour);
    p.putUChar("offtime2_to_minute", settings.offtime2.to_minute);
    p.putUChar("offtime2_days", settings.offtime2.days);
    // offtime 3
    p.putUChar("offtime3_from_hour", settings.offtime3.from_hour);
    p.putUChar("offtime3_from_minute", settings.offtime3.from_minute);
    p.putUChar("offtime3_to_hour", settings.offtime3.to_hour);
    p.putUChar("offtime3_to_minute", settings.offtime3.to_minute);
    p.putUChar("offtime3_days", settings.offtime3.days);
    // weather
    p.putDouble("weather_latitude", settings.weather_latitude);
    p.putDouble("weather_longitude", settings.weather_longitude);
    // timezone
    p.putString("timezone", settings.timezone);
    // anniversary
    p.putUChar("anniversary_day", settings.anniversary_day);
    p.putUChar("anniversary_month", settings.anniversary_month);

    p.end();
}

// portal

Portal::Portal() : server(), dns()
{
}

void Portal::start()
{
    // start DNS server
    if (!dns.start(53, "*", WiFi.softAPIP()))
    {
        Serial.println("[PORTAL] Failed to start DNS server");
        LittleFS.end();
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
    wifi_connect(cfg.ssid, cfg.password); // reconnect to the configured Wi-Fi

    Serial.println("[PORTAL] Server stopped");
}

void Portal::tick()
{
    // process DNS requests
    dns.processNextRequest();

    // No need to call server.handleClient() for AsyncWebServer
}

/// Server

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
    server.on("/api/settings", HTTP_GET, handle_get_settings);
    server.on("/api/networks", HTTP_GET, handle_get_networks);
    server.on("/api/settings", HTTP_DELETE, handle_delete_settings);
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, handle_post_settings);

    // start the server
    server.begin();

    Serial.println("[PORTAL] Server started");
}

void SettingsServer::stop()
{
    server.end(); // stop the HTTP server
    LittleFS.end();
}

// handler functions

static void handle_get_networks(AsyncWebServerRequest *request)
{
    String out;
    StaticJsonDocument<256> doc;
    JsonArray networksArray = doc.to<JsonArray>();
    // get wifi networks from last scan
    std::vector<NetworkInfo> networks = wifi_nearby_networks();
    for (const auto &network : networks)
    {
        JsonObject net = networksArray.createNestedObject();
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
    cfg.to_json(doc);
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

    // merge only what’s present
    cfg.from_json(doc);

    // save settings to persistent storage
    write_to_persistent_storage(cfg);
    req->send(200, "application/json", R"({"ok":true})");
}
