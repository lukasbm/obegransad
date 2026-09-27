#pragma once

#include "ikea-obegransad-panel.h"
#include "scene.h"
#include <math.h>

// screensaver scene where concretric circles are drawn
// this is a recreation of the original (unmodified) obegransad scene.
class RainScene : public Scene {
private:
  void draw() {
    panel_commit();
  }

protected:
  void render(uint32_t dt_ms) override {
    draw();
  }
};
