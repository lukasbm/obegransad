#include <Arduino.h>
#include "led.h"
#include "device.h"
#include "config.h"
#include "weather.h"
#include "clock.h"

class Scene
{
public:
    virtual void activate() {}
    virtual void deactivate() {}
    virtual void update() {}
};

class BrightnessScene : public Scene
{
public:
    void activate() override
    {
        panel_clear();
        // build a gradient for testing
        for (uint8_t i = 0; i < 16; i++)
        {
            for (uint8_t j = 0; j < 16; j++)
            {
                panel_setPixel(i, j, static_cast<uint8_t>(i * 16 + j));
            }
        }
        Serial.println("Brightness scene activated");
        panel_print();
    }
};

// A simple scene where a snake moves around the screen with a tail following
// moves once a second
// the tail becomes gradually dimmer (length 5)
// this is great for testing the LEDs
class SnakeScene : public Scene
{
private:
    int lastUpdateTime = 0;

    void drawSnake()
    {
        panel_clear();
        panel_show();
    }

    // pos is one of the 60 corner pixels. returns the x and y coordinates.
    // pos 0 is top left, moving clockwise
    void ring_coord(uint8_t pos, uint8_t &x, uint8_t &y)
    {
        if (pos < 16)
        {
            x = pos;
            y = 0;
        }
        else if (pos < 32)
        {
            x = 15;
            y = pos - 16;
        }
        else if (pos < 48)
        {
            x = 47 - pos;
            y = 15;
        }
        else
        {
            x = 0;
            y = 63 - pos;
        }
    }

public:
    void update() override
    {
        if (millis() > lastUpdateTime + 1000)
        {
            drawSnake();
            lastUpdateTime = millis();
        }
    }
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
        // panel_printChar(2, 0, (hour / 10) + 48);
        // panel_printChar(9, 0, (hour % 10) + 48);
        // panel_printChar(2, 9, (minute / 10) + 48);
        // panel_printChar(9, 9, (minute % 10) + 48);
        if (isNight())
        {
            // TODO: update total brightness
        }
    }

public:
    void activate() override
    {
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
    Scene scenes[1] = {
        BrightnessScene(),
        // WeatherScene(),
        // ClockScene()
    };
    int currentSceneIndex = -1; // -1 means no scene is active

public:
    void nextScene()
    {
        if (currentSceneIndex != -1)
        {
            scenes[currentSceneIndex].deactivate();
        }
        currentSceneIndex = (currentSceneIndex + 1) % 1; // FIXME: needs to be dynamic!!!!
        Serial.printf("Switching to scene %d\n", currentSceneIndex);
        // clear and switch
        panel_clear();
        panel_show();
        scenes[currentSceneIndex].activate();
    }

    void tick()
    {
        scenes[currentSceneIndex].update();
    }
};
