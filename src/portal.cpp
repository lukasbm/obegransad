#include "portal.h"

static Preferences prefs;
static DNSServer dns;
static AsyncWebServer server(80);

static bool portalActive = false;
static bool wantClose = false;

// try to connect with saved credentials
bool connectSTA(String &ssidOut)
{
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    if (ssid.isEmpty())
        return false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000)
        delay(50);
    ssidOut = ssid;
    return WiFi.status() == WL_CONNECTED;
}

void startPortal()
{
    WiFi.mode(WIFI_AP_STA); // AP + keep STA powered
    WiFi.softAP(AP_SSID, AP_PASS);

    dns.start(53, "*", WiFi.softAPIP()); // DNS hijack

    server.on("/", HTTP_ANY, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/done", HTTP_ANY, handleDone);
    server.on("/generate_204", HTTP_ANY,
              [](auto *r)
              { r->redirect("/"); }); // Android captive probe
    server.on("/hotspot-detect.html", HTTP_ANY,
              [](auto *r)
              { r->send(200, "text/html", "<!doctype html>"); });

    server.begin();
    portalActive = true;
    Serial.println("[PORTAL] started");
}

void startSoftAP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP("MyAP", "12345678"); // channel 1, default IP 192.168.4.1

    /* 1. DNS hijack – point **every** hostname to 192.168.4.1 */
    dns.start(53, "*", WiFi.softAPIP());

    /* 2. Captive-probe URLs (Android, Apple) */
    server.on("/generate_204", HTTP_ANY, []() { // Android
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "");
    });
    server.on("/hotspot-detect.html", HTTP_ANY, []() { // iOS
        server.send(200, "text/html", "<!doctype html>");
    });

    /* 3. Your app pages */
    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html");

    server.begin();
}

void stopPortal()
{
    server.end();
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    portalActive = false;
    wantClose = false;
    Serial.println("[PORTAL] closed");
}

static void server_handleWifi(AsyncWebServerRequest *req)
{
}

static void server_handleConfig(AsyncWebServerRequest *req)
{
}

void loadSettings()
{
    Preferences p;
    p.begin("app", true); // RO
    cfg.ssid = p.getString("ssid", "");
    cfg.brightness = p.getUChar("bright", 50);
    cfg.dhcp = p.getBool("dhcp", true);
    cfg.led = p.getBool("led", false);
    p.end();
}

void saveSettings()
{
    Preferences p;
    p.begin("app", false); // RW
    p.putString("ssid", cfg.ssid);
    p.putUChar("bright", cfg.brightness);
    p.putBool("dhcp", cfg.dhcp);
    p.putBool("led", cfg.led);
    p.end();
}

void setupApi()
{

    /* ----  GET returns full object  ---- */
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      StaticJsonDocument<256> doc;
      doc["ssid"]   = cfg.ssid;
      doc["bright"] = cfg.brightness;
      doc["dhcp"]   = cfg.dhcp;
      doc["led"]    = cfg.led;
      String out;  serializeJson(doc, out);
      req->send(200, "application/json", out); });

    /* ----  POST accepts JSON, merges only provided keys  ---- */
    server.on("/api/settings", HTTP_POST,
              /* onRequest */ [](AsyncWebServerRequest *req) {},
              /* onUpload  */ nullptr,
              /* onBody    */ [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
              {

      static String body;
      if (index == 0) body = "";                // first chunk
      body += String((char*)data, len);

      if (index + len == total) {               // last chunk
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, body) == DeserializationError::Ok) {

          /* merge only what’s present */
          if (doc.containsKey("ssid"))   cfg.ssid       = doc["ssid"  ].as<String>();
          if (doc.containsKey("bright")) cfg.brightness = doc["bright"].as<uint8_t>();
          if (doc.containsKey("dhcp"))   cfg.dhcp       = doc["dhcp"  ].as<bool>();
          if (doc.containsKey("led"))    cfg.led        = doc["led"   ].as<bool>();

          saveSettings();
          req->send(200, "application/json", R"({"ok":true})");
        } else
          req->send(400, "application/json", R"({"error":"bad json"})");
      } });

    /* ----  static assets  ---- */
    server.serveStatic("/", LittleFS, "/") // form.html.gz + CSS
        .setCacheControl("max-age=31536000")
        .setDefaultFile("form.html");
}
