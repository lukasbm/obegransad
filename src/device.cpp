#include "device.h"

WiFiManager wifiManager;
static const uint32_t RETRY_PERIOD_MS = 30000; // retry every 30 s
static unsigned long lastTry = 0;
static const char *PORTAL_NAME = "Obegransad";

// handles the cases when connection is lost in AP/STA mode
static void on_STA_disconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    static uint8_t failCount = 0;

    // print reason
    Serial.printf("Wi-Fi lost, reason %d … reconnecting\n",
                  info.wifi_sta_disconnected.reason);

    if (++failCount < 10)
    {
        WiFi.reconnect(); // try to reconnect
    }
    else
    {
        // 10×30 s ≈ 5 min of attempts → launch portal again
        failCount = 0;
        // FIXME: but i need/want WiFi symbol on
        wifiManager.startConfigPortal(PORTAL_NAME); // blocking portal
    }

    // quickest fix for most outages
    WiFi.disconnect(false); // keep radio on
    WiFi.reconnect();       // ↳ async, returns immediately
    lastTry = millis();     // remember when we tried
}

// FIXME: when is this used?
bool wifi_check(void)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - lastTry > RETRY_PERIOD_MS)
        {
            Serial.println("Wi-Fi lost, trying to reconnect …");
            lastTry = millis();
            return WiFi.reconnect();
        }
        else
        {
            return false; // not connected, but not yet time to retry
        }
    }
    else
    {
        // reset the timer when connected
        lastTry = 0;
        return true;
    }
}

void wifi_clear_credentials(void)
{
    // clear stored credentials
    wifiManager.resetSettings();
}

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
}

DeviceError wifi_setup(void)
{
    // if WiFi connection not in flash, start captive portal
    wifiManager.setConfigPortalTimeout(600); // 10 minutes
#if DEBUG
    wifiManager.setDebugOutput(true);
#endif

    if (WebServer *s = wifiManager.server.get())
    {
        add_captive_portal_spoof(s);
    }

    if (!wifiManager.autoConnect(PORTAL_NAME))
    {
        Serial.println("Portal timed out or aborted!");
        ESP.restart();
    }

    // register callback for connection loss
    WiFi.onEvent(on_STA_disconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.setAutoReconnect(true);

    // clean up after succes
    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    delay(5000);                 // give phone time to finish
    WiFi.softAPdisconnect(true); // drop the hotspot
    WiFi.mode(WIFI_STA);

    return ERR_NONE;
}

// TODO: migrate to WiFi off?
DeviceError enter_light_sleep(uint64_t seconds)
{
    // flush serial output
    Serial.flush();

    // flush sockets
    // TODO: flush async web server sockets from config?
    // TODO: maybe need to merge device and config module

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

    // enter sleep returns ESP_OK on wakeup)
    esp_err_t res = esp_light_sleep_start(); // returns ESP_OK on wakeup
    if (res != ESP_OK)
    {
        Serial.print("Failed to enter light sleep: ");
        Serial.println(esp_err_to_name(res));
        return ERR_SLEEP;
    }

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
        // will call on_STA_disconnected if it fails
        Serial.println("Failed to reconnect to WiFi after sleep");
        return ERR_WIFI;
    }

    return ERR_NONE;
}
