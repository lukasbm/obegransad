#include <Arduino.h>
#include <WiFi.h>
#include "weather.cpp"

// TODO:!
#define WIFI_SSID ""
#define WIFI_PASSWORD ""


void wifi_connect(void);

void setup()
{
  Serial.begin(115200);
  wifi_connect();
  esp_deep_sleep_start();
}

void loop()
{
}

void wifi_connect(void)
{
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
