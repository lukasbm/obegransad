#pragma once

#include "esp_log.h"
#include "helper.hpp"
#include "ikea-obegransad-panel.h"
#include "scene.h"

// A simple scene where a snake moves around the screen with a tail following
// moves once a second
// the tail becomes gradually dimmer (length 4)
// this is great for testing the LEDs
class SnakeScene : public Scene {
private:
  short headPos = 0;

  // TODO: make it move randomly instead of in a circle
  void drawSnake() {
    uint8_t x, y;

    ESP_LOGI("SnakeScene", "Snake head: %d", headPos);

    // head
    ring_coord((headPos + 60) % 60, x, y);
    panel_setPixel(y, x, PANEL_BRIGHTNESS_3);

    // tail
    ring_coord((headPos - 1 + 60) % 60, x, y);
    panel_setPixel(y, x, PANEL_BRIGHTNESS_3);

    ring_coord((headPos - 2 + 60) % 60, x, y);
    panel_setPixel(y, x, PANEL_BRIGHTNESS_2);

    ring_coord((headPos - 3 + 60) % 60, x, y);
    panel_setPixel(y, x, PANEL_BRIGHTNESS_1);

    panel_commit();
  }

public:
  const char *get_scene_name() const override { return "Snake"; }

  void update() override {
    static unsigned long lastUpdateTime = 0;

    if (millis() > lastUpdateTime + 1000) {
      panel_clear();
      drawSnake();
      headPos = (headPos + 1) % 60;
      lastUpdateTime = millis();
    }
  }
};
