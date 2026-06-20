#pragma once

#include "clock.h"
#include "helper.hpp"
#include "ikea-obegransad-panel.h"
#include "scene.h"
#include "sprites/thin_glyphs.hpp"

// bold clock scene
class ClockSceneWithSecondHand : public Scene {
private:
  int lastSecond = -1;

  void drawTime(uint8_t hour, uint8_t minute, uint8_t second = 0) {
    const uint8_t *sprite;

    panel_clear();

    // draw second hand around it
    uint8_t x, y;
    for (uint8_t i = 0; i < 60; i++) {
      ring_coord((i + 8) % 60, x,
                 y); // start at 8 to have the second hand at the center top
      if (i == second) {
        panel_setPixel(y, x, PANEL_BRIGHTNESS_3); // bright for second hand
      } else {
        panel_setPixel(y, x, PANEL_BRIGHTNESS_1); // dim for other pixels
      }
    }

    // hour first digit
    font_thin.drawGlyph((hour / 10) + 48, 3, 1); // hour/10 is 0,1 or 2

    // hour second digit
    font_thin.drawGlyph((hour % 10) + 48, 9, 1); // hour % 10 is 0-9

    // minute first digit
    font_thin.drawGlyph((minute / 10) + 48, 3, 9); // minute/10 is 0-5

    // minute second digit
    font_thin.drawGlyph((minute % 10) + 48, 9, 9); // minute % 10 is 0-9

    panel_commit();
  }

public:
  const char *get_scene_name() const override { return "Clock w/ seconds"; }
  uint16_t target_fps() const override { return 4; } // catch second changes
  bool is_clock() const override { return true; }

protected:
  void on_activate() override { lastSecond = -1; }

  void render(uint32_t /*dt_ms*/) override {
    struct tm time = time_get();
    if (time.tm_sec != lastSecond) {
      drawTime(time.tm_hour, time.tm_min, time.tm_sec);
      lastSecond = time.tm_sec;
    }
  }
};
