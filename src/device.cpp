#include "device.h"

void enter_light_sleep(uint64_t seconds)
{
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    gpio_wakeup_enable(WAKEUP_PIN, GPIO_INTR_LOW_LEVEL);
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
    // WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected!");
}

static struct tm timeinfo;

void time_setup()
{
    setenv("TZ", MY_TZ, 1); // set timezone
    tzset();                // apply timezone
}

void time_syncNTP()
{
    configTime(0, 0, MY_NTP_SERVER); // set NTP server
    if (!getLocalTime(&timeinfo, 2000))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println("NTP time synchronized.");
}

static void time_fetch()
{
    getLocalTime(&timeinfo, 2000);
}

int time_hour()
{
    time_fetch();
    return timeinfo.tm_hour;
}

int time_minute()
{
    time_fetch();
    return timeinfo.tm_min;
}

int time_second()
{
    time_fetch();
    return timeinfo.tm_sec;
}

bool isNight()
{
    time_fetch();
    if (timeinfo.tm_hour >= 22 || timeinfo.tm_hour < 6)
    {
        return true; // night
    }
    return false; // day
}

const char *getTimeString()
{
    static char timeString[20];
    snprintf(timeString, sizeof(timeString), "%02d:%02d:%02d", time_hour(), time_minute(), time_second());
    return timeString;
}

struct tm time_full()
{
    time_fetch();
    return timeinfo;
}
