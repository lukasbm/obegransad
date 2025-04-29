#include <Arduino.h>
#include "led.h"
#include "device.h"
#include "config.h"
#include "weather.h"

class Scene
{
public:
    virtual void activate() {}
    virtual void deactivate() {}
    virtual void update() {}
};

class WeatherScene : public Scene
{
private:
    struct WeatherData weatherData;
    int secondsSinceLastFetch = 0;
    int lastSecond = -1;

    void drawWeatherData()
    {
        // TODO: idk
    }

public:
    void activate() override
    {
        weatherData = fetchWeather(settings.weather_latitude, settings.weather_longitude);
    }

    void update() override
    {
        int currSecond = time_second();

        if (time_second() != lastSecond && secondsSinceLastFetch >= 65)
        {
            secondsSinceLastFetch++;
            if (secondsSinceLastFetch >= settings.weather_update_interval)
            {
                weatherData = fetchWeather(settings.weather_latitude, settings.weather_longitude);
                secondsSinceLastFetch = 0;
            }
            drawWeatherData();
        }

        lastSecond = currSecond;
    }
};

class ClockScene : public Scene
{
private:
    int lastMinute = -1;

    void drawTime(uint8_t hour, uint8_t minute)
    {
        panel_clear();
        panel_printChar(2, 0, (hour / 10) + 48);
        panel_printChar(9, 0, (hour % 10) + 48);
        panel_printChar(2, 9, (minute / 10) + 48);
        panel_printChar(9, 9, (minute % 10) + 48);
        if (isNight())
        {
            panel_show(1, settings.brightness_night); // refreshes display
        }
        else
        {
            panel_show(0, settings.brightness_day); // refreshes display
        }
    }

public:
    void activate() override
    {
        panel_debugTest();
        drawTime(time_hour(), time_minute());
    }

    void update() override
    {
        int currMinute = time_minute();

        if (currMinute != lastMinute)
        {
            drawTime(time_hour(), currMinute);
        }

        lastMinute = currMinute;
    }
};

class SceneSwitcher
{
private:
    Scene scenes[2] = {
        WeatherScene(),
        ClockScene()};
    int currentSceneIndex = -1; // -1 means no scene is active

public:
    void nextScene()
    {
        if (currentSceneIndex != -1)
        {
            scenes[currentSceneIndex].deactivate();
        }
        currentSceneIndex = (currentSceneIndex + 1) % 2;
        Serial.printf("Switching to scene %d\n", currentSceneIndex);
        scenes[currentSceneIndex].activate();
    }
    void tick()
    {
        scenes[currentSceneIndex].update();
    }
};
