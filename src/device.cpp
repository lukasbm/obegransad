#include "device.h"
#include "led.h"
#include "sprites/wifi.hpp"

WiFiManager wm;

static const char *PORTAL_NAME = "Obegransad";

// FIXME: do i really need the function?
// handles the cases when connection is lost in AP/STA mode
// the driver fires the event typically every 3-10 seconds when disconnected
static void on_STA_disconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    static uint8_t failCount = 0;

    // print reason
    Serial.printf("Driver Wi-Fi lost event (on_STA_disconnected), reason %d … reconnecting\n", info.wifi_sta_disconnected.reason);

    if (++failCount < 10)
    {
        Serial.printf("Reconnecting to Wi-Fi, attempt %d …\n", failCount);
        WiFi.reconnect();
    }
    else
    {
        Serial.println("Failed to reconnect, starting captive portal again…");
        failCount = 0;
        display_wifi_setup_prompt();                // show the wifi logo
        wm.startConfigPortal(PORTAL_NAME); // blocking until user fixes wifi.
    }
}

void display_wifi_setup_prompt(void)
{
    // display the wifi logo while connecting
    panel_clear();
    panel_drawSprite(3, 5, wifi_sprite.data, wifi_sprite.width, wifi_sprite.height);
    panel_show();
    panel_hold();
}

void display_device_error(DeviceError err)
{
    panel_clear();
    // TODO: draw error icons!!!
    panel_show();
    panel_hold();
}

bool wifi_check(void)
{
    static unsigned long lastTry = 0;

    if (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - lastTry > 30000)
        {
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
    wm.resetSettings();
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
}

DeviceError wifi_setup(void)
{
    // if WiFi connection not in flash, start captive portal
    wm.setConfigPortalTimeout(600); // 10 minutes

#if DEBUG
    wm.setDebugOutput(true);
#endif

    // make the captive portal actually appear on device
    if (WebServer *s = wm.server.get())
    {
        add_captive_portal_spoof(s);
    }

    Serial.println("Trying to open captive portal...");
    if (!wm.autoConnect(PORTAL_NAME))
    {
        Serial.println("Portal timed out or aborted!");
        ESP.restart();
    }

    // FIXME: register callback for connection loss
    // WiFi.onEvent(on_STA_disconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    // WiFi.setAutoReconnect(true); // one automatic retry after disconnect

    // clean up after success
    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    // migrate from AP to STA mode
    delay(3000);                 // give phone time to finish
    WiFi.softAPdisconnect(true); // drop the hotspot
    WiFi.mode(WIFI_STA);         // switch to STA mode

    return ERR_NONE;
}

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
        // will call on_STA_disconnected if it fails
        Serial.println("Failed to reconnect to WiFi after sleep");
        return ERR_WIFI;
    }

    return ERR_NONE;
}
