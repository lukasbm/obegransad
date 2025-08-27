#pragma once

class Scene {
public:
  virtual const char *get_scene_name() const = 0;

  virtual void activate() {};
  virtual void deactivate() {};
  virtual void update() {};

  // protected:
  //   void log_scene_event(const char *event) const {
  //     ESP_LOGI("scene", "%s: %s", get_scene_name(), event);
  //   }
};
