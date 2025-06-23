#include <Arduino.h>
#include "scene.h"
#include "weather.h"
#include "sprites/thin_glyphs.hpp"

// a line for the next 7 days forcast. temperature relative to current.
class WeatherMinMaxScene : public Scene
{
private:
    void drawWeatherData(const WeatherData &weatherData)
    {
        const uint8_t *sprite;

        panel_clear();

        /////////////// draw max temp at the top

        ////////////// draw min temp at the bottom
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
