#include "scene_switcher.h"

#include "ikea-obegransad-panel.h"
#include <array>
#include <list>
#include <vector>

// list of scenes
static std::list<Scene *> scenes;
static size_t current_scene_index = 0;
static bool wifi_available = false;

static bool is_scene_valid(Scene* s) {
    if (!s) return false;
    return wifi_available || !s->requires_wifi();
}

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
  
  size_t start_index = current_scene_index;
  size_t new_index = current_scene_index;
  
  do {
      new_index++;
      if (new_index >= scenes.size()) {
        new_index = 0;
      }
      
      auto it = std::next(scenes.begin(), new_index);
      if (is_scene_valid(*it)) {
          // Found a valid scene
           auto current_it = std::next(scenes.begin(), current_scene_index);
           (*current_it)->deactivate();
           
           current_scene_index = new_index;
           (*it)->activate();
           return;
      }
  } while (new_index != start_index);
}

void prev_scene() {
  if (scenes.empty())
    return;

  size_t start_index = current_scene_index;
  size_t new_index = current_scene_index;

  do {
      if (new_index == 0) {
        new_index = scenes.size() - 1;
      } else {
        new_index--;
      }
      
      auto it = std::next(scenes.begin(), new_index);
      if (is_scene_valid(*it)) {
           auto current_it = std::next(scenes.begin(), current_scene_index);
           (*current_it)->deactivate();
           
           current_scene_index = new_index;
           (*it)->activate();
           return;
      }
  } while (new_index != start_index);
}

void skipTo(size_t idx) {
  if (scenes.empty() || idx >= scenes.size())
    return;
    
  if (current_scene_index == idx) return;

  auto it = std::next(scenes.begin(), idx);
  if (!is_scene_valid(*it)) return; // Don't switch to invalid scene

  // Deactivate current
  auto current_it = std::next(scenes.begin(), current_scene_index);
  (*current_it)->deactivate();

  current_scene_index = idx;

  // Activate new
  (*it)->activate();
}

void tick() {
  if (scenes.empty()) return;
  // Get an iterator to the current scene
  auto it = std::next(scenes.begin(), current_scene_index);
  (*it)->update();
}

void scene_switcher_set_wifi_available(bool available) {
    wifi_available = available;
    
    if (scenes.empty()) return;

    auto it = std::next(scenes.begin(), current_scene_index);
    if (!is_scene_valid(*it)) {
        next_scene(); // Switch to next valid scene if current became invalid
    }
}

