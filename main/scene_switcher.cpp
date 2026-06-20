#include "scene_switcher.h"

#include "ikea-obegransad-panel.h"
#include <array>
#include <list>
#include <vector>

// list of scenes
static std::list<Scene *> scenes;
static size_t current_scene_index = 0;
static bool wifi_available = false;

// Auto-rotation alternates between clock and non-clock scenes. Each category
// keeps its own cursor (the index it last showed) so the two groups advance
// independently in list order; next_auto_clock decides which group comes next.
static int last_clock_index = -1;
static int last_nonclock_index = -1;
static bool next_auto_clock = false; // start on a non-clock, then a clock, ...

static bool is_scene_valid(Scene* s) {
    if (!s) return false;
    return wifi_available || !s->requires_wifi();
}

static Scene* scene_at(size_t idx) {
    return *std::next(scenes.begin(), idx);
}

// Find the next valid scene of the requested category (clock/non-clock),
// searching forward (with wraparound) starting just after `from`. Returns -1 if
// no valid scene of that category exists. `from` may be -1 to start at the top.
static int find_next_in_category(int from, bool want_clock) {
    const size_t n = scenes.size();
    if (n == 0) return -1;
    const size_t base = (from < 0) ? n - 1 : (size_t)from;
    for (size_t step = 1; step <= n; ++step) {
        const size_t idx = (base + step) % n;
        Scene* s = scene_at(idx);
        if (is_scene_valid(s) && s->is_clock() == want_clock) {
            return (int)idx;
        }
    }
    return -1;
}

// Switch the active scene to `idx` (deactivate current, activate target).
static void activate_scene(size_t idx) {
    if (idx == current_scene_index) return;
    scene_at(current_scene_index)->deactivate();
    current_scene_index = idx;
    scene_at(idx)->activate();
}

// After a manual move, realign the auto cursors to the current scene so that
// auto-rotation resumes cleanly with the opposite category next.
static void sync_auto_cursors_to_current() {
    if (scenes.empty()) return;
    const bool is_clk = scene_at(current_scene_index)->is_clock();
    if (is_clk) {
        last_clock_index = (int)current_scene_index;
    } else {
        last_nonclock_index = (int)current_scene_index;
    }
    next_auto_clock = !is_clk;
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
           sync_auto_cursors_to_current();
           return;
      }
  } while (new_index != start_index);
}

void next_auto_scene() {
  if (scenes.empty()) return;

  bool want_clock = next_auto_clock;
  int from = want_clock ? last_clock_index : last_nonclock_index;
  int target = find_next_in_category(from, want_clock);

  // Desired category has no valid scene right now (e.g. clocks vs. a list with
  // none, or non-clock-only while Wi-Fi-gated scenes are filtered). Fall back to
  // the other category so rotation keeps moving, and try the wanted one again
  // next time.
  if (target < 0) {
    want_clock = !want_clock;
    from = want_clock ? last_clock_index : last_nonclock_index;
    target = find_next_in_category(from, want_clock);
  }
  if (target < 0) return;

  activate_scene((size_t)target);
  if (want_clock) {
    last_clock_index = target;
  } else {
    last_nonclock_index = target;
  }
  next_auto_clock = !want_clock; // alternate for the next interval
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
           sync_auto_cursors_to_current();
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
  sync_auto_cursors_to_current();
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

