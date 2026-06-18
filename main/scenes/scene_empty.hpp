#pragma once

#include "ikea-obegransad-panel.h"
#include "scene.h"

class EmptyScene : public Scene {
public:
  const char *get_scene_name() const override { return "Empty"; }
  uint16_t target_fps() const override { return 0; } // static

protected:
  void render(uint32_t /*dt_ms*/) override {
    panel_clear();
    panel_commit();
  }
};
