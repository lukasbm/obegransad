#include "device.h"

#include <Arduino.h>
#include <string.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "led.h"
#include "sprites/wifi.hpp"

static WiFiManager wm;

static const char *PORTAL_NAME = "Obegransad-Setup"; // captive portal name

// handles the cases when connection is lost in AP/STA mode
// the driver fires the event typically every 3-10 seconds when disconnected
static void callback_STA_disconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    static uint8_t failCount = 0;

    // print reason
    Serial.printf("Driver Wi-Fi lost event (callback_STA_disconnected), reason %d … reconnecting\n", info.wifi_sta_disconnected.reason);

    if (++failCount < 10)
    {
        Serial.printf("Reconnecting to Wi-Fi, attempt %d …\n", failCount);
        WiFi.reconnect();
    }
    else
    {
        Serial.println("Failed to reconnect, starting captive portal again…");
        failCount = 0;
        display_wifi_logo();               // show the wifi logo
        wm.startConfigPortal(PORTAL_NAME); // blocking until user fixes wifi.
    }
}

bool wifi_check()
{
    return (WiFi.status() == WL_CONNECTED);
}

void wifi_clear_credentials(void)
{
    // clear stored credentials
    wm.resetSettings();
    ESP.restart(); // restart the device to apply changes
}

// need to call captive portal setup first
bool wifi_setup(void)
{
    WiFi.onEvent(callback_STA_disconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    return wm.autoConnect(PORTAL_NAME);
}

// add some endpoints to trick android and IOS into showing the captive portal
// this is needed to make the captive portal appear on the device
static void add_captive_portal_spoof(WebServer *s)
{
    s->on(
        "/generate_204",
        HTTP_ANY,
        [s]()
        {
      s->sendHeader("Location", "/");
      s->send(302, "text/plain", ""); });

    s->on(
        "/hotspot-detect.html",
        HTTP_ANY,
        [s]()
        {
            s->send(200, "text/html", "<!doctype html>");
        });

    // Additional endpoints for better captive portal detection
    s->on("/fwlink", HTTP_ANY, [s]()
          {
        s->sendHeader("Location", "/");
        s->send(302, "text/plain", ""); });

    s->on("/connecttest.txt", HTTP_ANY, [s]()
          { s->send(200, "text/plain", "Microsoft Connect Test"); });
}

void captive_portal_tick()
{
    // this is called in the main loop to keep the captive portal alive
    // it will handle button presses and other events
    wm.process();
}

void captive_portal_start()
{
    // Need to disconnect first to ensure clean AP setup
    WiFi.disconnect();

    if (!captive_portal_active())
        wm.startConfigPortal(PORTAL_NAME);
}

bool captive_portal_active()
{
    return wm.getConfigPortalActive();
}

void captive_portal_stop()
{
    // stop the captive portal
    if (captive_portal_active())
        wm.stopConfigPortal();
}

// Sets up wifi and starts the captive portal if no credentials are stored
// make sure this function is never called more than once!
void captive_portal_setup(void)
{
    wm.setConfigPortalBlocking(false); // has to be the first statement, otherwise it will not work
    wm.setWiFiAutoReconnect(true);
    wm.setConfigPortalTimeout(120); // Don't timeout
    wm.setConnectTimeout(30);       // seconds to connect to Wi-Fi
    wm.setDarkMode(true);
#if DEBUG
    wm.setDebugOutput(true);
#endif
    // Callback when AP (portal) comes up
    wm.setAPCallback([](WiFiManager *w)
                     { Serial.println(">>> Entered config portal"); });

    // Callback when credentials are saved (Exit button)
    wm.setSaveConfigCallback([]()
                             { Serial.println(">>> Credentials saved; portal should stop soon"); });

    // make the captive portal actually appear on device
    if (WebServer *s = wm.server.get())
    {
        add_captive_portal_spoof(s);
    }
}

// Make sure to flush sockets (e.g. web server and http client) before entering light sleep.
DeviceError enter_light_sleep(uint64_t seconds)
{
    // flush serial output
    Serial.flush();

    // stop wifi
    if (esp_wifi_stop() != ESP_OK)
    {
        Serial.println("Failed to stop WiFi");
        return ERR_WIFI;
    }

    // set up wake timer
    if (esp_sleep_enable_timer_wakeup(seconds * 1000000ULL) != ESP_OK)
    {
        Serial.println("Failed to set timer wakeup");
        return ERR_SLEEP;
    }

    // set up wake up sources (button)
    if (gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_PIN), GPIO_INTR_LOW_LEVEL) != ESP_OK)
    {
        Serial.printf("Failed to set GPIO wakeup for pin %d\n", BUTTON_PIN);
        return ERR_SLEEP;
    }

    // enable GPIO wakeup
    if (esp_sleep_enable_gpio_wakeup() != ESP_OK)
    {
        Serial.println("Failed to enable GPIO wakeup");
        return ERR_SLEEP;
    }

    // enter sleep (returns ESP_OK on wakeup)
    esp_err_t res = esp_light_sleep_start();
    if (res != ESP_OK)
    {
        Serial.print("Failed to enter light sleep: ");
        Serial.println(esp_err_to_name(res));
        return ERR_SLEEP;
    }

    // zZZzzz ... blocks during sleep ...

    // examine wakeup reason
    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    Serial.printf("Wakeup cause: %d\n", reason);

    // bring back wifi
    if (esp_wifi_start() != ESP_OK)
    {
        Serial.println("Failed to start WiFi after sleep");
        return ERR_WIFI;
    }

    // re-auth to wifi
    if (!WiFi.reconnect())
    {
        // will call callback_STA_disconnected if it fails
        Serial.println("Failed to reconnect to WiFi after sleep");
        return ERR_WIFI;
    }

    return ERR_NONE;
}
