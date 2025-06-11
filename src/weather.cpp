#include <HTTPClient.h>

#include "weather.h"
#include "config.h"

static WeatherData cachedWeatherData;

const WeatherData &weather_get()
{
    if (cachedWeatherData.weatherCode == WEATHER_UNINITIALIZED)
    {
        // Fetch weather data only if it has not been fetched yet
        weather_fetch();
        Serial.println("Weather data fetched from API:");
        cachedWeatherData.print();
    }
    return cachedWeatherData;
}

static void parseWeatherData(WeatherData &res, const JsonDocument &doc)
{
    auto current = doc["current"];
    res.temperature = float(current["temperature_2m"]);
    res.weatherCode = WeatherCode(current["weather_code"]);
    res.isDay = bool(current["is_day"]);

    auto daily = doc["daily"];
    for (int i = 0; i < FORECAST_DAYS; ++i)
    {
        res.daily[i].sunrise = uint32_t(daily["sunrise"][i]);
        res.daily[i].sunset = uint32_t(daily["sunset"][i]);
        res.daily[i].uvIndexMax = float(daily["uv_index_max"][i]);
        res.daily[i].temperatureMax = float(daily["temperature_2m_max"][i]);
        res.daily[i].temperatureMin = float(daily["temperature_2m_min"][i]);
        res.daily[i].temperatureMean = float(daily["temperature_2m_mean"][i]);
        res.daily[i].weatherCode = WeatherCode(daily["weather_code"][i]);
    }
}

void weather_fetch()
{
    WeatherData res; // invalid data by default

    // 1) Build request URL
    // Example URL: https://api.open-meteo.com/v1/forecast?latitude=49.4542&longitude=11.0775&daily=sunrise,sunset,uv_index_max,temperature_2m_max,temperature_2m_min,weather_code,temperature_2m_mean&current=temperature_2m,weather_code,is_day&timezone=auto&timeformat=unixtime
    String url = String("https://api.open-meteo.com/v1/forecast") +
                 "?latitude=" + String(gSettings.weather_latitude) +
                 "&longitude=" + String(gSettings.weather_longitude) +
                 "&daily=sunrise,sunset,uv_index_max,temperature_2m_max,temperature_2m_min,weather_code,temperature_2m_mean" +
                 "&current=temperature_2m,weather_code,is_day" +
                 "&timezone=auto" +
                 "&timeformat=unixtime" +
                 "&forecast_days=" + String(FORECAST_DAYS);

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

    cachedWeatherData = res; // cache the result
}
