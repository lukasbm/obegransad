#include "portal.h"
#include "device.h"
#include "config.h"

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstdint>
#include <Preferences.h>
#include <DNSServer.h>
#include <WiFi.h>

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
    settings.to_json(doc);
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
