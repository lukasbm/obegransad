#include <variant>
#include <Arduino.h>
#include "led.h"
#include "device.h"
#include "config.h"
#include "weather.h"
#include "clock.h"
#include "sprites/small_test.hpp"
#include "sprites/bold_glyphs.hpp"
#include "sprites/thin_glyphs.hpp"
#include "sprites/cloud.hpp"
#include "sprites/sun.hpp"
#include "sprites/rain.hpp"

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

class SpriteTestScene : public Scene
{
public:
    void activate() override
    {
        uint8_t const *sprite;

        Serial.println("Sprite test scene activated");
        panel_clear();

        sprite = font_bold.getGlyph('1');
        panel_drawSprite(0, 0, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
        sprite = font_bold.getGlyph('9');
        panel_drawSprite(0, 8, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
        sprite = font_bold.getGlyph('A');
        panel_drawSprite(9, 0, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
        sprite = font_bold.getGlyph('Y');
        panel_drawSprite(9, 8, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
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
        if (pos < 16) // 16 in the top row
        {
            x = pos;
            y = 0;
        }
        else if (pos < 30) // 16 in the top row and 14 down
        {
            x = 15;
            y = pos - 15;
        }
        else if (pos < 46)
        {
            x = 45 - pos;
            y = 15;
        }
        else
        {
            x = 0;
            y = 60 - pos;
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

    void drawWeatherData()
    {
        panel_clear();

        const uint8_t *sprite;

        ////////////////// temperature at the bottom (bottom 6 pixels)

        // minus sign
        if (weatherData.temperature < 0)
        {
            sprite = font_thin.getGlyph(45); // minus sign
            panel_drawSprite(0, 9, sprite, font_thin.spriteWidth, font_thin.spriteHeight);
        }

        int temperature = (int)abs(weatherData.temperature);
        // first digit
        sprite = font_thin.getGlyph(temperature / 10 + 48); // temperature/10 is 0-2
        panel_drawSprite(4, 9, sprite, font_thin.spriteWidth, font_thin.spriteHeight);
        // second digit
        sprite = font_thin.getGlyph(temperature % 10 + 48);
        panel_drawSprite(9, 9, sprite, font_thin.spriteWidth, font_thin.spriteHeight);

        // degree symbol
        panel_setPixel(9, 14, BRIGHTNESS_4);
        panel_setPixel(9, 15, BRIGHTNESS_4);
        panel_setPixel(10, 14, BRIGHTNESS_4);
        panel_setPixel(10, 15, BRIGHTNESS_4);

        // draw separator (1 pixel row in the middle)
        for (int i = 0; i < 16; i++)
        {
            panel_setPixel(8, i, BRIGHTNESS_4);
        }

        ///////////////// TODO: symbols at top (top 9 pixels)

        // cloud
        sprite = data_cloud;
        panel_drawSprite(0, 0, sprite, 9, 5);

        // rain
        sprite = animation_rain.nextFrame();
        panel_drawSprite(9, 0, sprite, 7, 8);
    }

public:
    void activate() override
    {
        Serial.println("Weather scene activated");
        // initial data
        weatherData = fetchWeather(settings.weather_latitude, settings.weather_longitude);
        weatherData.print();
        drawWeatherData();
    }

    void update() override
    {
        static unsigned long lastFetch = millis();
        static unsigned long lastDraw = 0;

        const int weatherUpdateInterval = 600; // 10 minutes

        unsigned long now = millis();
        if (now - lastFetch > weatherUpdateInterval * 1000)
        {
            weatherData = fetchWeather(settings.weather_latitude, settings.weather_longitude);
            weatherData.print();
            lastFetch = now;
        }
        // have to draw all the time (every second) because of animations
        if (now - lastDraw > 200)
        {
            lastDraw = now;
            drawWeatherData();
        }
    }
};

class ClockScene : public Scene
{
private:
    void drawTime(uint8_t hour, uint8_t minute)
    {
        const uint8_t *sprite;

        panel_clear();

        // hour first digit
        sprite = font_bold.getGlyph((hour / 10) + 48); // hour/10 is 0,1 or 2
        panel_drawSprite(2, 0, sprite, font_bold.spriteWidth, font_bold.spriteHeight);

        // hour second digit
        sprite = font_bold.getGlyph((hour % 10) + 48); // hour % 10 is 0-9
        panel_drawSprite(9, 0, sprite, font_bold.spriteWidth, font_bold.spriteHeight);

        // minute first digit
        sprite = font_bold.getGlyph((minute / 10) + 48); // minute/10 is 0-5
        panel_drawSprite(2, 9, sprite, font_bold.spriteWidth, font_bold.spriteHeight);

        // minute second digit
        sprite = font_bold.getGlyph((minute % 10) + 48); // minute % 10 is 0-9
        panel_drawSprite(9, 9, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
    }

public:
    void activate() override
    {
        Serial.println("Clock scene activated");
        struct tm time = time_fetch();
        drawTime(time.tm_hour, time.tm_min);
    }

    void update() override
    {
        static int lastMinute = -1;

        struct tm time = time_fetch();
        if (time.tm_min != lastMinute)
        {
            drawTime(time.tm_hour, time.tm_min);
            lastMinute = time.tm_min;
        }
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
    // SpriteTestScene spriteTestScene;
    SnakeScene snakeScene;
    WeatherScene weatherScene;
    ClockScene clockScene;
    static constexpr uint8_t numScenes = 4;

    // pointers for polymorphic dispatch
    const std::array<Scene *, numScenes> scenes;
    uint8_t currIdx;
};
