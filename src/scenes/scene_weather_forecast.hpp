#include <Arduino.h>
#include "scene.h"
#include "weather.h"
#include "sprites/thin_glyphs.hpp"

// regular temp at top, then a line for the next 7 days forcast. temperature relative to current.
class WeatherForecastScene : public Scene
{
public:
    void activate() override
    {
    }

    void update() override
    {
    }
};
