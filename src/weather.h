#pragma once

#include <ArduinoJson.h>

constexpr uint8_t FORECAST_DAYS = 7; // number of days to forecast, used in weather_fetch

// from: https://open-meteo.com/en/docs#weather_variable_documentation
enum WeatherCode : uint8_t
{
    CLEAR = 0,                            // sprite: clear sky
    PARTLY_CLOUDY = 1,                    // sprite: cloud sprite
    CLOUDY = 2,                           // sprite: cloud sprite
    OVERCAST = 3,                         // sprite: cloud sprite
    FOG = 45,                             // sprite: cloud sprite
    RIME_FOG = 48,                        // sprite: cloud sprite
    LIGHT_DRIZZLE = 51,                   // sprite: raining
    MODERATE_DRIZZLE = 53,                // sprite: raining
    HEAVY_DRIZZLE = 55,                   // sprite: raining
    LIGHT_FREEZING_DRIZZLE = 56,          // sprite: raining
    DENSE_FREEZING_DRIZZLE = 57,          // sprite: raining
    SLIGHT_RAIN = 61,                     // sprite: raining
    MODERATE_RAIN = 63,                   // sprite: raining
    HEAVY_RAIN = 65,                      // sprite: raining
    LIGHT_FREEZING_RAIN = 66,             // sprite: raining
    HEAVY_FREEZING_RAIN = 67,             // sprite: raining
    SLIGHT_SNOW_FALL = 71,                // sprite: snow
    MODERATE_SNOW_FALL = 73,              // sprite: snow
    HEAVY_SNOW_FALL = 75,                 // sprite: snow
    SNOW_GRAINS = 77,                     // sprite: snow
    SLIGHT_RAIN_SHOWER = 80,              // sprite: raining
    MODERATE_RAIN_SHOWER = 81,            // sprite: raining
    VIOLENT_RAIN_SHOWER = 82,             // sprite: raining
    SLIGHT_SNOW_SHOWER = 85,              // sprite: snow
    HEAVY_SNOW_SHOWER = 86,               // sprite: snow
    SLIGHT_OR_MODERATE_THUNDERSTORM = 95, // sprite: lightning bolts
    THUNDERSTORM_WITH_LIGHT_HAIL = 96,    // sprite: lightning bolts
    THUNDERSTORM_WITH_HEAVY_HAIL = 99,    // sprite: lightning bolts
    UNKNOWN = 100,                        // custom placeholder for unknown weather codes
    UNINITIALIZED = 255                   // custom placeholder for uninitialized weather codes
};

struct WeatherData
{
    uint32_t requestTime; // time when the weather data was fetched, unix time
    // current weather
    float temperature;
    WeatherCode weatherCode = UNINITIALIZED; // default to uninitialized
    bool isDay;
    // daily weather
    struct
    {
        uint32_t sunrise; // unixtime
        uint32_t sunset;  // unixtime
        float uvIndexMax;
        float temperatureMax;
        float temperatureMin;
        float temperatureMean;
        WeatherCode weatherCode;
    } daily[FORECAST_DAYS]; // daily weather for the next days

    void print() const
    {
        Serial.printf("Weather Data: RequestTime: %u, temperature: %.2f, weatherCode: %d, isDay: %d\n",
                      requestTime, temperature, weatherCode, isDay);
        for (int i = 0; i < FORECAST_DAYS; ++i)
        {
            Serial.printf("Day %d: sunrise: %u, sunset: %u, uvIndexMax: %.2f, temperatureMax: %.2f, temperatureMin: %.2f, temperatureMean: %.2f, weatherCode: %d\n",
                          i + 1, daily[i].sunrise, daily[i].sunset, daily[i].uvIndexMax,
                          daily[i].temperatureMax, daily[i].temperatureMin, daily[i].temperatureMean,
                          daily[i].weatherCode);
        }
    }
};

WeatherData weather_fetch(float lat, float lon);
const WeatherData &weather_get();
