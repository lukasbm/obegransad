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

// A simple scene where a snake moves around the screen with a tail following
// moves once a second
// the tail becomes gradually dimmer (length 5)
// this is great for testing the LEDs
class SnakeScene : public Scene
{
private:
    int x = 0;
    int y = 0;
    int tail[5][2] = {0};
    int tailLength = 5;

    void drawSnake()
    {
        panel_clear();
        for (int i = 0; i < tailLength; i++)
        {
            panel_setPixel(tail[i][0], tail[i][1], 255 - (i * 50));
        }
        panel_setPixel(x, y, 255);
        panel_show();
    }

public:
    void update() override
    {
        // move the snake
        x = (x + 1) % 16;
        y = (y + 1) % 16;

        // move the tail
        for (int i = tailLength - 1; i > 0; i--)
        {
            tail[i][0] = tail[i - 1][0];
            tail[i][1] = tail[i - 1][1];
        }
        tail[0][0] = x;
        tail[0][1] = y;

        drawSnake();
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
        panel_printChar(2, 0, (hour / 10) + 48);
        panel_printChar(9, 0, (hour % 10) + 48);
        panel_printChar(2, 9, (minute / 10) + 48);
        panel_printChar(9, 9, (minute % 10) + 48);
        if (isNight())
        {
            panel_show(); // refreshes display
        }
        else
        {
            panel_show(); // refreshes display
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
        panel_clear();
        scenes[currentSceneIndex].activate();
    }
    void tick()
    {
        scenes[currentSceneIndex].update();
    }
};
