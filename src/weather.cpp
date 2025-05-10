#include <HTTPClient.h>

#include "weather.h"

struct WeatherData fetchWeather(float lat, float lon)
{
    WeatherData res;

    // 1) Build request URL
    // BETTER example url (with weather codes, needed for icons!): https://api.open-meteo.com/v1/forecast?latitude=49.4542&longitude=11.0775&daily=uv_index_max&hourly=temperature_2m&current=precipitation,rain,showers,snowfall,temperature_2m,weather_code,cloud_cover&timezone=auto&forecast_days=1
    String url = String("https://api.open-meteo.com/v1/forecast") +
                 "?latitude=" + String(lat) +
                 "&longitude=" + String(lon) +
                 "&daily=uv_index_max" +
                 //  "&hourly=uv_index,temperature_2m" +
                 "&current=temperature_2m,rain,precipitation,showers,snowfall,weather_code,cloud_cover" +
                 "&timezone=auto" +
                 "&forecast_days=1";

    HTTPClient http;
    http.begin(url); // HTTPS by default

    // 2) Send the request
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
        String payload = http.getString();

        // 3) Parse JSON
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err)
        {
            // 4) Extract data
            parseWeatherData(res, doc);
        }
        else
        {
            Serial.print("JSON parse error: ");
            Serial.println(err.c_str());
        }
    }
    else
    {
        Serial.printf("HTTP error: %d\n", httpCode);
    }
    http.end();

    return res;
}

static void parseWeatherData(WeatherData &res, const JsonDocument &doc)
{
    JsonObjectConst current = doc["current"];
    res.precipitation = current["precipitation"];
    res.rain = current["rain"];
    res.showers = current["showers"];
    res.snowfall = current["snowfall"];
    res.temperature = current["temperature_2m"];
    res.weatherCode = (WeatherCode)current["weather_code"];

    JsonObjectConst daily = doc["daily"];
    res.maxUvIndex = daily["uv_index_max"][0];
}
