#pragma once

#include "config.h"
#include "helper.hpp"
#include "ikea-obegransad-panel.h"
#include "scene.h"
#include "sprites/cloud.hpp"
#include "sprites/rain.hpp"
#include "sprites/thin_glyphs.hpp"
#include "weather.h"

class WeatherScene : public Scene {
private:
  struct RainAnimation animation_rain;

  void drawWeatherData(const WeatherData &weatherData) {
    panel_clear();

    const uint8_t *sprite;

    ////////////////// temperature at the bottom (bottom 6 pixels)

    // minus sign
    if (weatherData.temperature < 0) {
      font_thin.drawGlyph(45, 0, 9); // minus sign
    }

    int temperature = (int)abs(weatherData.temperature);
    // first digit
    font_thin.drawGlyph(temperature / 10 + 48, 4, 9); // temperature/10 is 0-2
    // second digit
    font_thin.drawGlyph(temperature % 10 + 48, 9, 9);

    // degree symbol
    panel_setPixel(9, 14, PANEL_BRIGHTNESS_3);
    panel_setPixel(9, 15, PANEL_BRIGHTNESS_3);
    panel_setPixel(10, 14, PANEL_BRIGHTNESS_3);
    panel_setPixel(10, 15, PANEL_BRIGHTNESS_3);

    // draw separator (1 pixel row in the middle)
    for (int i = 0; i < 16; i++) {
      panel_setPixel(8, i, PANEL_BRIGHTNESS_3);
    }

    ///////////////// TODO: symbols at top based on weather code (top 9 pixels)

    // cloud
    cloud_sprite.draw(0, 0);

    // rain
    animation_rain.drawNextFrame(9, 0);

    panel_commit();
  }

  // Shown while online but no weather data has arrived yet (or fetch failing).
  void drawWaiting() {
    panel_clear();
    font_thin.drawGlyph('-', 4, 5);
    font_thin.drawGlyph('-', 9, 5);
    panel_commit();
  }

public:
  const char *get_scene_name() const override { return "Current Weather"; }
  uint16_t target_fps() const override { return 5; } // 200 ms animations
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
