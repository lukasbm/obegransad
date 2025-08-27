#pragma once

#include "ikea-obegransad-panel.h"
#include "scene.h"
#include "sprites/bold_glyphs.hpp"

class SpriteTestScene : public Scene {
public:
  const char *get_scene_name() const override { return "Test"; }

  void activate() override {
    panel_clear();

    font_bold.drawGlyph('1', 0, 0);
    font_bold.drawGlyph('9', 0, 8);
    font_bold.drawGlyph('A', 9, 0);
    font_bold.drawGlyph('Y', 9, 8);

    panel_commit();
  }
};
