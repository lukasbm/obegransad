#include "scene_switcher.h"

#include "ikea-obegransad-panel.h"
#include <array>
#include <list>
#include <vector>

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

void next_scene() {
  if (scenes.empty())
    return;
  
  // Deactivate current
  auto it = std::next(scenes.begin(), current_scene_index);
  (*it)->deactivate();

  current_scene_index++;
  if (current_scene_index >= scenes.size()) {
    current_scene_index = 0;
  }
  
  // Activate new
  it = std::next(scenes.begin(), current_scene_index);
  (*it)->activate();
}

void prev_scene() {
  if (scenes.empty())
    return;

  // Deactivate current
  auto it = std::next(scenes.begin(), current_scene_index);
  (*it)->deactivate();

  if (current_scene_index == 0) {
    current_scene_index = scenes.size() - 1;
  } else {
    current_scene_index--;
  }

  // Activate new
  it = std::next(scenes.begin(), current_scene_index);
  (*it)->activate();
}

void skipTo(size_t idx) {
  if (scenes.empty() || idx >= scenes.size())
    return;
    
  if (current_scene_index == idx) return;

  // Deactivate current
  auto it = std::next(scenes.begin(), current_scene_index);
  (*it)->deactivate();

  current_scene_index = idx;

  // Activate new
  it = std::next(scenes.begin(), current_scene_index);
  (*it)->activate();
}

void tick() {
  if (scenes.empty()) return;
  // Get an iterator to the current scene
  auto it = std::next(scenes.begin(), current_scene_index);
  (*it)->update();
}

