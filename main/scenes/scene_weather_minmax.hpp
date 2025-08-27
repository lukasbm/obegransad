#pragma once

#include "scene.h"
#include "sprites/thin_glyphs.hpp"
#include "weather.h"

// a line for the next 7 days forcast. temperature relative to current.
class WeatherMinMaxScene : public Scene {
private:
  void drawWeatherData(const WeatherData &weatherData) {
    const uint8_t *sprite;

    panel_clear();

    /////////////// draw max temp at the top

    ////////////// draw min temp at the bottom

    panel_commit();
  }

public:
  const char *get_scene_name() const override { return "Daily Weather"; }

  void update() override {
    static RenderTimer timer(20000); // ms timer for animations

    if (timer.check()) {
      drawWeatherData(weather_get());
    }
  }
};
