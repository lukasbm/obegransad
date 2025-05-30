#include "device.h"
#include "led.h"
#include "sprites/wifi.hpp"

WiFiManager wifiManager;

static const char *PORTAL_NAME = "Obegransad-Setup"; // captive portal name (SSID)

// handles the cases when connection is lost in AP/STA mode
// the driver fires the event typically every 3-10 seconds when disconnected
static void callback_sta_disconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    static uint8_t failCount = 0;

    // print reason
    Serial.printf("Driver Wi-Fi lost event (callback_sta_disconnected), reason %d … reconnecting\n", info.wifi_sta_disconnected.reason);

    if (++failCount < 10)
    {
        Serial.printf("Reconnecting to Wi-Fi, attempt %d …\n", failCount);
        WiFi.reconnect();
    }
    else
    {
        Serial.println("Failed to reconnect, starting captive portal again…");
        failCount = 0;
        display_wifi_symbol();                      // show the wifi logo
        wifiManager.startConfigPortal(PORTAL_NAME); // blocking until user fixes wifi.
    }
}

void display_wifi_symbol(void)
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
    // return WiFi.status == WL_CONNECTED; // FIXME: would this be enough as there is the onSTADisconnected event?

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
    wifiManager.resetSettings();
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

void wifi_setup(void)
{
    // configure arduino WiFi abstraction lib
    WiFi.mode(WIFI_AP_STA);                                                       // set mode to AP+STA
    WiFi.setHostname("Obegransad");                                               // set hostname
    WiFi.onEvent(callback_sta_disconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED); // register callback
    WiFi.setAutoReconnect(true);                                                  // one automatic retry after disconnect

    // configure the Wi-Fi manager (uses WiFi interally)
    wifiManager.setConfigPortalBlocking(true); // blocking mode
    wifiManager.setConfigPortalTimeout(6);     // FIXME: 1 min
#if DEBUG
    wifiManager.setDebugOutput(true);
#endif

    // hack to make the captive portal actually appear on device
    if (WebServer *s = wifiManager.server.get())
    {
        add_captive_portal_spoof(s);
    }

    Serial.println("opening captive portal...");
    if (!wifiManager.autoConnect(PORTAL_NAME))
    {
        Serial.println("Portal timed out or aborted!");
    }

    // clean up after success
    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    // migrate from AP to STA mode
    delay(3000);                 // give phone time to finish
    WiFi.softAPdisconnect(true); // drop the hotspot
    WiFi.mode(WIFI_STA);         // switch to STA mod
}

DeviceError enter_light_sleep(uint64_t seconds)
{
    // flush serial output
    Serial.flush();

    // flush sockets
    // TODO: flush async web server sockets from config?
    // maybe need to merge device and config module

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
        // will call callback_sta_disconnected if it fails
        Serial.println("Failed to reconnect to WiFi after sleep");
        return ERR_WIFI;
    }

    return ERR_NONE;
}

std::vector<NetworkInfo> wifi_nearby_networks(void)
{
    Serial.println("Scanning for nearby Wi-Fi networks...");

    // start scan
    if (WiFi.scanNetworks(true, true) < 0)
    {
        Serial.println("Failed to start Wi-Fi scan");
        return;
    }

    // wait for scan to complete
    while (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    {
        delay(100);
    }

    // print results
    int numNetworks = WiFi.scanComplete();
    Serial.printf("Found %d networks:\n", numNetworks);

    std::vector<NetworkInfo> sortedNetworks; // vector to hold sorted networks
    sortedNetworks.clear();                  // clear the sorted networks list
    for (int i = 0; i < numNetworks; ++i)
    {
        // create a network info object
        NetworkInfo netInfo;
        netInfo.ssid = WiFi.SSID(i);
        netInfo.rssi = WiFi.RSSI(i);
        netInfo.quality = wifi_rssi_quality(netInfo.rssi); // calculate quality
        netInfo.encryptionType = WiFi.encryptionType(i);
        netInfo.channel = WiFi.channel(i);
        // add to the sorted list
        sortedNetworks.push_back(netInfo);
    }

    // reset scan results
    WiFi.scanDelete();

    return sortedNetworks; // return the sorted networks
}

uint8_t wifi_rssi_quality(int rssi)
{
    int quality = 0;
    if (rssi <= -100)
    {
        quality = 0;
    }
    else if (rssi >= -50)
    {
        quality = 100;
    }
    else
    {
        quality = 2 * (rssi + 100);
    }
    return quality;
}

void wifi_connect(const String &ssid, const String &password)
{
    Serial.printf("Connecting to Wi-Fi SSID: %s\n", ssid.c_str());

    WiFi.mode(WIFI_STA); // ensure we are in STA mode
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - startTime > 10000) // timeout after 10 seconds
        {
            Serial.println("Failed to connect to Wi-Fi");
            return;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}
void wifi_disconnect()
{
    Serial.println("Disconnecting from Wi-Fi...");
    WiFi.disconnect();
    Serial.println("Disconnected from Wi-Fi");
}
void wifi_reconnect()
{
    Serial.println("Reconnecting to Wi-Fi...");
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            if (millis() - startTime > 10000) // timeout after 10 seconds
            {
                Serial.println("Failed to reconnect to Wi-Fi");
                return;
            }
            delay(500);
            Serial.print(".");
        }
        Serial.println("\nReconnected to Wi-Fi!");
    }
}

// void onGotIP(WiFiEvent_t, WiFiEventInfo_t) {
//     if (WiFi.getMode() == WIFI_AP_STA) {        // still hosting AP?
//         WiFi.softAPdisconnect(true);            // stop beacons
//         WiFi.mode(WIFI_STA);                    // pure station mode
//     }
// }
