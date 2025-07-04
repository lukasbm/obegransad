#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "weather.h"
#include "config.h"
#include "sprites/thin_glyphs.hpp"
#include "sprites/cloud.hpp"
#include "sprites/rain.hpp"
#include "scenes/helper.hpp"

class WeatherScene : public Scene
{
private:
    struct RainAnimation animation_rain;

    void drawWeatherData(const WeatherData &weatherData)
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
    }

    void update() override
    {
        static RenderTimer timer(200); // 200 ms timer for animations

        if (timer.check())
        {
            drawWeatherData(weather_get());
        }
    }
};
