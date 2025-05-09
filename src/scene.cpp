#include <variant>
#include <Arduino.h>
#include "led.h"
#include "device.h"
#include "config.h"
#include "weather.h"
#include "clock.h"

// avoid calling panel_clear in the scene.
// It is called in the SceneManager/Switcher.
// This way we can combine multiple scenes!
class Scene
{
public:
    virtual void activate() {}
    virtual void deactivate() {}
    virtual void update() {}
};

class EmptyScene : public Scene
{
public:
    void activate() override
    {
        panel_clear();
        Serial.println("Empty scene activated");
    }
};

class BrightnessScene : public Scene
{
public:
    void activate() override
    {
        Serial.println("Brightness scene activated");
        for (uint8_t y = 0; y < 16; y++)
            for (uint8_t x = 0; x < 16; x++)
                panel_setPixel(y, x, y * 16 + x);
        Serial.println("Brightness scene activated");
        panel_print();
    }
};

// A simple scene where a snake moves around the screen with a tail following
// moves once a second
// the tail becomes gradually dimmer (length 4)
// this is great for testing the LEDs
class SnakeScene : public Scene
{
private:
    unsigned long lastUpdateTime = 0;
    short headPos = 0;

    void drawSnake()
    {
        uint8_t x, y;

        Serial.printf("Snake head: %d\n", headPos);

        // head
        ring_coord((headPos + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_4);

        ring_coord((headPos - 1 + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_3);

        ring_coord((headPos - 2 + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_2);

        ring_coord((headPos - 3 + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_1);
    }

    // pos is one of the 60 corner pixels. writes the x and y coordinates.
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
            y = pos - 15;
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
    void activate() override
    {
        Serial.println("Snake Scene activated");
    }
    void update() override
    {
        if (millis() > lastUpdateTime + 1000)
        {
            panel_clear();
            drawSnake();
            headPos = (headPos + 1) % 60;
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
        Serial.println("Weather scene activated");
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
        // panel_printChar(2, 0, (hour / 10) + 48);
        // panel_printChar(9, 0, (hour % 10) + 48);
        // panel_printChar(2, 9, (minute / 10) + 48);
        // panel_printChar(9, 9, (minute % 10) + 48);
    }

public:
    void activate() override
    {
        Serial.println("Clock scene activated");
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
public:
    SceneSwitcher() : scenes{&emptyScene, &snakeScene, &weatherScene, &clockScene}, currIdx(0)
    {
        Serial.println("SceneSwitcher initialized");
        scenes[currIdx]->activate();
    }

    ~SceneSwitcher()
    {
        scenes[currIdx]->deactivate();
    }

    void nextScene()
    {
        scenes[currIdx]->deactivate();
        currIdx = (currIdx + 1) % numScenes;
        panel_clear();
        scenes[currIdx]->activate();
    }

    void prevScene()
    {
        scenes[currIdx]->deactivate();
        currIdx = (currIdx + numScenes - 1) % numScenes;
        panel_clear();
        scenes[currIdx]->activate();
    }

    void tick()
    {
        scenes[currIdx]->update();
    }

private:
    // all scenes live here, in RAM
    EmptyScene emptyScene;
    SnakeScene snakeScene;
    WeatherScene weatherScene;
    ClockScene clockScene;
    static constexpr uint8_t numScenes = 4;

    // pointers for polymorphic dispatch
    const std::array<Scene *, numScenes> scenes;
    uint8_t currIdx;
};
