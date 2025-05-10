#pragma once

#include <ArduinoJson.h>

// from: https://open-meteo.com/en/docs#weather_variable_documentation
// The weather panel is split in two parts. The left is the general view of the sky and the right side gives precipitation data
enum WeatherCode : uint8_t
{
    CLEAR = 0,                            // full: clear sky
    PARTLY_CLOUDY = 1,                    // left: cloud sprite
    CLOUDY = 2,                           // left: cloud sprite
    OVERCAST = 3,                         // left: cloud sprite
    FOG = 45,                             // left: cloud sprite
    RIME_FOG = 48,                        // left: cloud sprite
    LIGHT_DRIZZLE = 51,                   // right: raining
    MODERATE_DRIZZLE = 53,                // right: raining
    HEAVY_DRIZZLE = 55,                   // right: raining
    LIGHT_FREEZING_DRIZZLE = 56,          // right: raining
    DENSE_FREEZING_DRIZZLE = 57,          // right: raining
    SLIGHT_RAIN = 61,                     // right: raining
    MODERATE_RAIN = 63,                   // right: raining
    HEAVY_RAIN = 65,                      // right: raining
    LIGHT_FREEZING_RAIN = 66,             // right: raining
    HEAVY_FREEZING_RAIN = 67,             // right: raining
    SLIGHT_SNOW_FALL = 71,                // right: snowing
    MODERATE_SNOW_FALL = 73,              // right: snowing
    HEAVY_SNOW_FALL = 75,                 // right: snowing
    SNOW_GRAINS = 77,                     // right: snowing
    SLIGHT_RAIN_SHOWER = 80,              // right: raining
    MODERATE_RAIN_SHOWER = 81,            // right: raining
    VIOLENT_RAIN_SHOWER = 82,             // right: raining
    SLIGHT_SNOW_SHOWER = 85,              // right: snowing
    HEAVY_SNOW_SHOWER = 86,               // right: snowing
    SLIGHT_OR_MODERATE_THUNDERSTORM = 95, // right: lightning bolts
    THUNDERSTORM_WITH_LIGHT_HAIL = 96,    // right: lightning bolts
    THUNDERSTORM_WITH_HEAVY_HAIL = 99,    // right: lightning bolts
};

struct WeatherData
{
    float precipitation;
    float rain;
    float showers;
    float snowfall;
    float temperature;
    WeatherCode weatherCode;
    float maxUvIndex;
};

struct WeatherData fetchWeather(float lat, float lon);
static void parseWeatherData(WeatherData &res, const JsonDocument &doc);
