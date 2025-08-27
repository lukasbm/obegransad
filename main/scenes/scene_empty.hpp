#pragma once

#include "ikea-obegransad-panel.h"
#include "scene.h"

class EmptyScene : public Scene {
public:
  const char *get_scene_name() const override { return "Empty"; }

  void activate() override {
    panel_clear();
    panel_commit();
  }
};
