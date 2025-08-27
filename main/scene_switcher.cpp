#include "scene_switcher.h"

#include "ikea-obegransad-panel.h"
#include <array>
#include <list>
#include <vector>

#include "scenes/scene_test.hpp"

// list of scenes
static std::list<Scene *> scenes;
static size_t current_scene_index = 0;

void register_scene(Scene *scene) { scenes.push_back(scene); }

void unregister_scene(Scene *scene) {
  scenes.remove(scene);
  if (current_scene_index >= scenes.size()) {
    current_scene_index = 0; // reset index if it was out of bounds
  }
}

void next_scene() {}

void prev_scene() {}

void skipTo(size_t idx) {}

void tick() {
  // Get an iterator to the current scene
  auto it = std::next(scenes.begin(), current_scene_index);
  (*it)->update();
}

SpriteTestScene test_scene;

