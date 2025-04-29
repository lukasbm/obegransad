#include <HTTPClient.h>
#include <ArduinoJson.h>

// example url: https://api.open-meteo.com/v1/forecast?latitude=49.4542&longitude=11.0775&daily=uv_index_max,sunrise,sunset&hourly=uv_index&current=temperature_2m,rain,precipitation,showers,snowfall,wind_speed_10m&timezone=auto&forecast_days=1

struct WeatherData
{
    float temperature;
    float rain;
    float precipitation;
    float showers;
    float snowfall;
    float maxUvIndex;
    float windSpeed;
};

struct WeatherData fetchWeather(float lat, float lon)
{
    WeatherData res;

    // 1) Build request URL
    String url = String("https://api.open-meteo.com/v1/forecast") +
                 "?latitude=" + String(lat) +
                 "&longitude=" + String(lon) +
                 "&daily=uv_index_max,sunrise,sunset" +
                 "&hourly=uv_index&current=temperature_2m,rain,precipitation,showers,snowfall,wind_speed_10m&timezone=auto&forecast_days=1";

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
    JsonObject current = doc["current"];
    res.temperature = current["temperature_2m"];
    res.rain = current["rain"];
    res.precipitation = current["precipitation"];
    res.showers = current["showers"];
    res.snowfall = current["snowfall"];
    res.windSpeed = current["wind_speed_10m"];

    JsonObject daily = doc["daily"];
    res.maxUvIndex = daily["uv_index_max"][0];
}
