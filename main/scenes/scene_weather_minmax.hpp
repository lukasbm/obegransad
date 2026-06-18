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

  void drawWaiting() {
    panel_clear();
    font_thin.drawGlyph('-', 4, 5);
    font_thin.drawGlyph('-', 9, 5);
    panel_commit();
  }

public:
  const char *get_scene_name() const override { return "Daily Weather"; }
  uint16_t target_fps() const override { return 1; }
  bool requires_wifi() const override { return true; }

protected:
  void render(uint32_t /*dt_ms*/) override {
    if (!weather_is_valid()) {
      drawWaiting();
      return;
    }
    drawWeatherData(weather_get());
  }
};
