#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "weather.h"
#include "config.h"
#include "sprites/thin_glyphs.hpp"
#include "sprites/cloud.hpp"
#include "sprites/rain.hpp"

class WeatherScene : public Scene
{
private:
    struct WeatherData weatherData;
    struct RainAnimation animation_rain;

    void drawWeatherData()
    {
        panel_clear();

        const uint8_t *sprite;

        ////////////////// temperature at the bottom (bottom 6 pixels)

        // minus sign
        if (weatherData.temperature < 0)
        {
            sprite = thin_font.getGlyph(45); // minus sign
            panel_drawSprite(0, 9, sprite, thin_font.spriteWidth, thin_font.spriteHeight);
        }

        int temperature = (int)abs(weatherData.temperature);
        // first digit
        sprite = thin_font.getGlyph(temperature / 10 + 48); // temperature/10 is 0-2
        panel_drawSprite(4, 9, sprite, thin_font.spriteWidth, thin_font.spriteHeight);
        // second digit
        sprite = thin_font.getGlyph(temperature % 10 + 48);
        panel_drawSprite(9, 9, sprite, thin_font.spriteWidth, thin_font.spriteHeight);

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

        ///////////////// TODO: symbols at top based on weather code (top 9 pixels)

        // cloud
        panel_drawSprite(0, 0, cloud_sprite.data, cloud_sprite.width, cloud_sprite.height);

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

    // TODO: move fetching to weather module!
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
