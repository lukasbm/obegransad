#pragma once

#include "ikea-obegransad-panel.h"
#include "scene.h"
#include "sdkconfig.h"
#include "sprites/heart.hpp"
#include "sprites/thin_glyphs.hpp"

// scene to display anniversary dates with a heart animation
class AnniversaryScene : public Scene {
private:
  HeartAnimation animation_heart;

  void drawHeart(uint8_t day, uint8_t month) {
    const uint8_t *sprite;

    panel_clear();

    // update animation
    animation_heart.drawNextFrame(0, 0);

    // day first digit
    font_thin.drawGlyph((day / 10) + 48, 0, 10); // day/10 is 0,1 or 2
    // day second digit
    font_thin.drawGlyph((day % 10) + 48, 4, 10); // day % 10 is 0-9
    // month first digit
    font_thin.drawGlyph((month / 10) + 48, 8, 10); // month/10 is 0-5
    // month second digit
    font_thin.drawGlyph((month % 10) + 48, 12, 10); // month % 10 is 0-9

    panel_commit();
  }

public:
  const char *get_scene_name() const override { return "Anniversary"; }
  uint16_t target_fps() const override { return 4; }

protected:
  void render(uint32_t /*dt_ms*/) override {
    drawHeart(CONFIG_OBG_ANNIVERSARY_DAY, CONFIG_OBG_ANNIVERSARY_MONTH);
  }
};
