#include <Arduino.h>
#include "scene.h"
#include "weather.h"
#include "sprites/thin_glyphs.hpp"
#include "scenes/helper.hpp"

// regular temp at top, then a line for the next 7 days forcast. temperature relative to current.
class WeatherForecastScene : public Scene
{
private:
    void drawWeatherData(const WeatherData &weatherData)
    {
        const uint8_t *sprite;

        panel_clear();

        //////////// draw current temp at the top

        // minus sign
        if (weatherData.temperature < 0)
        {
            thin_font.drawGlyph(45, 0, 0); // minus sign
        }
        int temperature = (int)abs(weatherData.temperature);
        // first digit
        thin_font.drawGlyph(temperature / 10 + 48, 4, 0); // temperature/10 is 0-2
        // second digit
        thin_font.drawGlyph(temperature % 10 + 48, 9, 0);

        // degree symbol
        panel_setPixel(0, 14, BRIGHTNESS_3);
        panel_setPixel(0, 15, BRIGHTNESS_3);
        panel_setPixel(1, 14, BRIGHTNESS_3);
        panel_setPixel(1, 15, BRIGHTNESS_3);

        /////////////// draw forecast for the next days
        const uint8_t center = 7;

        // we have enough space for 5 forecasts
        for (uint8_t i = 0; i < 5; i++)
        {
            uint8_t row = 9 + 2 * i;
            // draw center point
            panel_setPixel(row, center, BRIGHTNESS_3);
            // draw difference to todays max
            int diff = int(weatherData.daily[i + 1].temperatureMax - weatherData.daily[0].temperatureMax); // difference to todays max
            if (diff > 0)
            {
                // draw positive difference
                for (int j = 1; j <= diff && center + j < 16; j++)
                {
                    panel_setPixel(row, center + j, BRIGHTNESS_3);
                }
            }
            else if (diff < 0)
            {
                // draw negative difference
                for (int j = -1; j >= diff && center + j >= 0; j--)
                {
                    panel_setPixel(row, center + j, BRIGHTNESS_3);
                }
            }
        }

        panel_commit();
    }

public:
    void activate() override
    {
        Serial.println("Weather scene activated");
    }

    void update() override
    {
        static RenderTimer timer(20000); // ms timer for animations

        if (timer.check())
        {
            drawWeatherData(weather_get());
        }
    }
};
