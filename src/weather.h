#pragma once

#include <HTTPClient.h>
#include <ArduinoJson.h>

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

struct WeatherData fetchWeather(float lat, float lon);
static void parseWeatherData(WeatherData &res, const JsonDocument &doc);

// from: https://open-meteo.com/en/docs#weather_variable_documentation
enum WeatherCode
{
    CLEAR = 0,
    PARTLY_CLOUDY = 1,
    CLOUDY = 2,
    OVERCAST = 3,
    FOG = 45,
    RIME_FOG = 48,
    LIGHT_DRIZZLE = 51,
    MODERATE_DRIZZLE = 53,
    HEAVY_DRIZZLE = 55,
    LIGHT_FREEZING_DRIZZLE = 56,
    DENSE_FREEZING_DRIZZLE = 57,
    SLIGHT_RAIN = 61,
    MODERATE_RAIN = 63,
    HEAVY_RAIN = 65,
    LIGHT_FREEZING_RAIN = 66,
    HEAVY_FREEZING_RAIN = 67,
    SLIGHT_SNOW_FALL = 71,
    MODERATE_SNOW_FALL = 73,
    HEAVY_SNOW_FALL = 75,
    SNOW_GRAINS = 77,
    SLIGHT_RAIN_SHOWER = 80,
    MODERATE_RAIN_SHOWER = 81,
    VIOLENT_RAIN_SHOWER = 82,
    SLIGHT_SNOW_SHOWER = 85,
    HEAVY_SNOW_SHOWER = 86,
    SLIGHT_OR_MODERATE_THUNDERSTORM = 95,
    THUNDERSTORM_WITH_LIGHT_HAIL = 96,
    THUNDERSTORM_WITH_HEAVY_HAIL = 99,
};
