#include "device.h"

void wifi_clear_credentials(void)
{
    // clear stored credentials
    WiFiManager wifiManager;
    wifiManager.resetSettings();
}

void setup_device(void)
{
    // setup button as input
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // if WiFi connection not in flash, start captive portal
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(600); // 10 minutes

// wipe stored credentials (stored in flash by esp library)
#if DEBUG
    wifiManager.setDebugOutput(true);
#endif

    bool res = wifiManager.autoConnect("Obengransad");
    if (!res)
    {
        Serial.println("Failed to connect to WiFi");
        delay(3000);
        // restart the ESP32
        ESP.restart();
    }

    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

void enter_light_sleep(uint64_t seconds)
{
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_PIN), GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    // turn of wifi
    // wifi_off();

    // enter sleep
    esp_err_t res = esp_light_sleep_start(); // returns ESP_OK on wakeup
    if (res != ESP_OK)
    {
        Serial.print("Failed to enter light sleep: ");
        Serial.println(esp_err_to_name(res));
        return;
    }

    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    Serial.printf("Wakeup cause: %d\n", reason);

    // restore wifi (it should reconnect automatically though)
    // wifi_connect();
}

void wifi_connect(void)
{
    // turn on and connect to wifi
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    // WiFi.setSleep(false);        // Disable WiFi sleep mode
    WiFi.setAutoReconnect(true); // Enable auto-reconnect
    WiFi.setHostname("ESP32-Obengransad-Clock");
    // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // Serial.print("Connecting to ");
    // Serial.println(WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");

    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void wifi_off(void)
{
    Serial.println("Disconnecting from WiFi...");
    // WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected!");
}
