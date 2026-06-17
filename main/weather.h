#pragma once

#include "esp_err.h"
#include <cJSON.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <time.h>

// TODO: move to kconfig!
constexpr uint8_t FORECAST_DAYS = 7; // number of days to forecast

// from: https://open-meteo.com/en/docs#weather_variable_documentation
enum WeatherCode : uint8_t {
  WEATHER_CLEAR = 0,                            // sprite: clear sky
  WEATHER_PARTLY_CLOUDY = 1,                    // sprite: cloud sprite
  WEATHER_CLOUDY = 2,                           // sprite: cloud sprite
  WEATHER_OVERCAST = 3,                         // sprite: cloud sprite
  WEATHER_FOG = 45,                             // sprite: cloud sprite
  WEATHER_RIME_FOG = 48,                        // sprite: cloud sprite
  WEATHER_LIGHT_DRIZZLE = 51,                   // sprite: raining
  WEATHER_MODERATE_DRIZZLE = 53,                // sprite: raining
  WEATHER_HEAVY_DRIZZLE = 55,                   // sprite: raining
  WEATHER_LIGHT_FREEZING_DRIZZLE = 56,          // sprite: raining
  WEATHER_DENSE_FREEZING_DRIZZLE = 57,          // sprite: raining
  WEATHER_SLIGHT_RAIN = 61,                     // sprite: raining
  WEATHER_MODERATE_RAIN = 63,                   // sprite: raining
  WEATHER_HEAVY_RAIN = 65,                      // sprite: raining
  WEATHER_LIGHT_FREEZING_RAIN = 66,             // sprite: raining
  WEATHER_HEAVY_FREEZING_RAIN = 67,             // sprite: raining
  WEATHER_SLIGHT_SNOW_FALL = 71,                // sprite: snow
  WEATHER_MODERATE_SNOW_FALL = 73,              // sprite: snow
  WEATHER_HEAVY_SNOW_FALL = 75,                 // sprite: snow
  WEATHER_SNOW_GRAINS = 77,                     // sprite: snow
  WEATHER_SLIGHT_RAIN_SHOWER = 80,              // sprite: raining
  WEATHER_MODERATE_RAIN_SHOWER = 81,            // sprite: raining
  WEATHER_VIOLENT_RAIN_SHOWER = 82,             // sprite: raining
  WEATHER_SLIGHT_SNOW_SHOWER = 85,              // sprite: snow
  WEATHER_HEAVY_SNOW_SHOWER = 86,               // sprite: snow
  WEATHER_SLIGHT_OR_MODERATE_THUNDERSTORM = 95, // sprite: lightning bolts
  WEATHER_THUNDERSTORM_WITH_LIGHT_HAIL = 96,    // sprite: lightning bolts
  WEATHER_THUNDERSTORM_WITH_HEAVY_HAIL = 99,    // sprite: lightning bolts
  WEATHER_INVALID = 255 // custom placeholder for uninitialized codes
};

struct WeatherData {
  time_t requestTime; // time when the weather data was fetched, unix time
  // current weather
  float temperature;
  WeatherCode weatherCode = WEATHER_INVALID; // default to uninitialized
  bool isDay;
  // daily weather
  struct {
    time_t sunrise; // unix time
    time_t sunset;  // unix time
    float uvIndexMax;
    float temperatureMax;
    float temperatureMin;
    float temperatureMean;
    WeatherCode weatherCode;
  } daily[FORECAST_DAYS]; // daily weather for the next days

  void print() const;
};

esp_err_t fetch_weather(float latitude, float longitude, WeatherData &data);

// Cached weather accessors. fetch_weather() populates the cache via
// weather_set(); scenes read the most recent snapshot via weather_get().
// TODO: drive weather_set() from a periodic weather client task (see
// DEVELOPER.md "Weather client").
WeatherData weather_get();
void weather_set(const WeatherData &data);
