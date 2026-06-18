#pragma once

#include "config.h"
#include "helper.hpp"
#include "scene.h"
#include "sprites/thin_glyphs.hpp"
#include "weather.h"

// regular temp at top, then a line for the next 7 days forcast. temperature
// relative to current.
class WeatherForecastScene : public Scene {
private:
  void drawWeatherData(const WeatherData &weatherData) {
    panel_clear();

    //////////// draw current temp at the top

    // minus sign
    if (weatherData.temperature < 0) {
      font_thin.drawGlyph(45, 0, 0); // minus sign
    }
    int temperature = (int)abs(weatherData.temperature);
    // first digit
    font_thin.drawGlyph(temperature / 10 + 48, 4, 0); // temperature/10 is 0-2
    // second digit
    font_thin.drawGlyph(temperature % 10 + 48, 9, 0);

    // degree symbol
    panel_setPixel(0, 14, PANEL_BRIGHTNESS_3);
    panel_setPixel(0, 15, PANEL_BRIGHTNESS_3);
    panel_setPixel(1, 14, PANEL_BRIGHTNESS_3);
    panel_setPixel(1, 15, PANEL_BRIGHTNESS_3);

    /////////////// draw forecast for the next days
    const uint8_t center = 7;

    // we have enough space for 5 forecasts
    for (uint8_t i = 0; i < 5; i++) {
      uint8_t row = 9 + 2 * i;
      // draw center point
      panel_setPixel(row, center, PANEL_BRIGHTNESS_3);
      // draw difference to todays max
      int diff =
          int(weatherData.daily[i + 1].temperatureMax -
              weatherData.daily[0].temperatureMax); // difference to todays max
      if (diff > 0) {
        // draw positive difference
        for (int j = 1; j <= diff && center + j < 16; j++) {
          panel_setPixel(row, center + j, PANEL_BRIGHTNESS_3);
        }
      } else if (diff < 0) {
        // draw negative difference
        for (int j = -1; j >= diff && center + j >= 0; j--) {
          panel_setPixel(row, center + j, PANEL_BRIGHTNESS_3);
        }
      }
    }

    panel_commit();
  }

  void drawWaiting() {
    panel_clear();
    font_thin.drawGlyph('-', 4, 5);
    font_thin.drawGlyph('-', 9, 5);
    panel_commit();
  }

public:
  const char *get_scene_name() const override { return "Weather Forecast"; }
  uint16_t target_fps() const override { return 1; }
  bool requires_wifi() const override { return true; }

protected:
  // Reads the shared cache populated by the background weather client; never
  // fetches here (that would block the render path on a 30 s HTTPS request).
  void render(uint32_t /*dt_ms*/) override {
    if (!weather_is_valid()) {
      drawWaiting();
      return;
    }
    drawWeatherData(weather_get());
  }
};
