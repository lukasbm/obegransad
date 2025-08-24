#pragma once

class Scene {
public:
  virtual const char *get_scene_name() const = 0;

  virtual void activate() = 0;
  virtual void deactivate() = 0;
  virtual void update() = 0;
};
