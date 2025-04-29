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
