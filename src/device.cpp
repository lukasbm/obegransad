#include <WiFi.h>
#include <esp_sleep.h>

#define WAKEUP_PIN GPIO_NUM_9 // button pin!

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

void sleep(uint64_t seconds)
{
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    gpio_wakeup_enable(WAKEUP_PIN, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    // turn of wifi
    wifi_off();

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

    // restore wifi
    wifi_connect();
}

void wifi_connect(void)
{
    // turn on and connect to wifi
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    // WiFi.setSleep(false);        // Disable WiFi sleep mode
    WiFi.setAutoReconnect(true); // Enable auto-reconnect
    WiFi.setHostname("ESP32-Obengransad-Clock");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

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

void wifi_off()
{
    Serial.println("Disconnecting from WiFi...");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected!");
}
