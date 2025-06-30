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
            thin_font.drawGlyph(45, 0, 9); // minus sign
        }

        int temperature = (int)abs(weatherData.temperature);
        // first digit
        thin_font.drawGlyph(temperature / 10 + 48, 4, 9); // temperature/10 is 0-2
        // second digit
        thin_font.drawGlyph(temperature % 10 + 48, 9, 9);

        // degree symbol
        panel_setPixel(9, 14, BRIGHTNESS_3);
        panel_setPixel(9, 15, BRIGHTNESS_3);
        panel_setPixel(10, 14, BRIGHTNESS_3);
        panel_setPixel(10, 15, BRIGHTNESS_3);

        // draw separator (1 pixel row in the middle)
        for (int i = 0; i < 16; i++)
        {
            panel_setPixel(8, i, BRIGHTNESS_3);
        }

        ///////////////// TODO: symbols at top based on weather code (top 9 pixels)

        // cloud
        cloud_sprite.draw(0, 0);

        // rain
        animation_rain.drawNextFrame(9, 0);

        panel_commit();
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
