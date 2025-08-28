#pragma once

#include "esp_log.h"

class Scene {
public:
  virtual const char *get_scene_name() const = 0;

  virtual void activate() { log_scene_event("activated"); };
  virtual void deactivate() { log_scene_event("deactivated"); };
  virtual void update() {};

protected:
  void log_scene_event(const char *event) const {
    ESP_LOGI("scene", "%s: %s", get_scene_name(), event);
  }
};
