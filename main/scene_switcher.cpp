#include "scene_switcher.h"

#include "presets.hpp"

#include <cstring>
#include <esp_log.h>
#include <vector>

static const char *TAG = "scenes";

namespace {

// All registered scenes, in registration order.
std::vector<Scene *> scenes;

// Preset scene names resolved to indices into `scenes`. Resolved once at init
// so the rotation never touches strings.
int8_t resolved[PRESET_COUNT][MAX_PRESET_SCENES];
uint8_t resolved_count[PRESET_COUNT];

uint8_t current_preset = 0;
uint8_t position_in_preset = 0; // index into resolved[current_preset]
int current_scene_index = -1;   // index into `scenes`, -1 = nothing active
bool on_fallback = false;       // showing the fallback, not a preset scene
bool wifi_available = false;

bool is_scene_valid(int idx) {
  if (idx < 0 || (size_t)idx >= scenes.size()) {
    return false;
  }
  return wifi_available || !scenes[idx]->requires_wifi();
}

int scene_index_by_name(const char *name) {
  for (size_t i = 0; i < scenes.size(); i++) {
    if (strcmp(scenes[i]->get_scene_name(), name) == 0) {
      return (int)i;
    }
  }
  return -1;
}

// Shown when the active preset has no scene we can display right now (e.g. a
// weather-only preset while Wi-Fi is down).
int fallback_scene_index() {
  for (size_t i = 0; i < scenes.size(); i++) {
    if (!scenes[i]->requires_wifi()) {
      return (int)i;
    }
  }
  return scenes.empty() ? -1 : 0;
}

void activate_index(int idx) {
  if (idx < 0 || (size_t)idx >= scenes.size() || idx == current_scene_index) {
    return;
  }
  if (current_scene_index >= 0) {
    scenes[current_scene_index]->deactivate();
  }
  current_scene_index = idx;
  scenes[idx]->activate();
}

// First valid position at or after `from` (wrapping) within the active preset,
// or -1 if the preset has nothing displayable right now.
int find_valid_position(uint8_t from) {
  const uint8_t count = resolved_count[current_preset];
  for (uint8_t step = 0; step < count; step++) {
    const uint8_t pos = (from + step) % count;
    if (is_scene_valid(resolved[current_preset][pos])) {
      return (int)pos;
    }
  }
  return -1;
}

// Show the preset starting at `from`, falling back if nothing in it is valid.
void show_preset_from(uint8_t from) {
  const int pos = resolved_count[current_preset] ? find_valid_position(from) : -1;
  if (pos < 0) {
    ESP_LOGW(TAG, "preset %u '%s' has no displayable scene; using fallback",
             current_preset, presets[current_preset].name);
    on_fallback = true;
    activate_index(fallback_scene_index());
    return;
  }
  on_fallback = false;
  position_in_preset = (uint8_t)pos;
  activate_index(resolved[current_preset][pos]);
}

void select_preset(uint8_t index) {
  current_preset = index;
  ESP_LOGI(TAG, "preset %u: %s (%u scene(s), dwell %ums)", current_preset,
           presets[current_preset].name, resolved_count[current_preset],
           (unsigned)presets[current_preset].dwell_ms);
  show_preset_from(0);
}

// Next non-empty preset in the given direction, wrapping. Returns the current
// one if no other preset has any scenes.
uint8_t step_preset(int direction) {
  int candidate = current_preset;
  for (uint8_t step = 1; step <= PRESET_COUNT; step++) {
    candidate = (candidate + direction + PRESET_COUNT) % PRESET_COUNT;
    if (resolved_count[candidate] > 0) {
      return (uint8_t)candidate;
    }
  }
  return current_preset;
}

} // namespace

void register_scene(Scene *scene) {
  if (scene) {
    scenes.push_back(scene);
  }
}

void scene_switcher_init() {
  memset(resolved_count, 0, sizeof(resolved_count));

  for (uint8_t p = 0; p < PRESET_COUNT; p++) {
    const Preset &preset = presets[p];
    const uint8_t wanted = preset.scenes ? preset.scene_count : 0;
    if (wanted > MAX_PRESET_SCENES) {
      ESP_LOGE(TAG, "preset %u '%s' lists %u scenes, max is %u; truncating", p,
               preset.name, wanted, MAX_PRESET_SCENES);
    }
    for (uint8_t i = 0; i < wanted && i < MAX_PRESET_SCENES; i++) {
      const int idx = scene_index_by_name(preset.scenes[i]);
      if (idx < 0) {
        ESP_LOGE(TAG, "preset %u '%s': unknown scene \"%s\", skipped", p,
                 preset.name, preset.scenes[i]);
        continue;
      }
      resolved[p][resolved_count[p]++] = (int8_t)idx;
    }
    if (resolved_count[p] > 0) {
      ESP_LOGI(TAG, "preset %u '%s': %u scene(s), dwell %ums", p, preset.name,
               resolved_count[p], (unsigned)preset.dwell_ms);
    }
  }

  select_preset(0);
}

uint8_t preset_current() { return current_preset; }

void preset_next() { select_preset(step_preset(+1)); }

void preset_prev() { select_preset(step_preset(-1)); }

uint32_t preset_dwell_ms() { return presets[current_preset].dwell_ms; }

void rotation_advance() {
  if (resolved_count[current_preset] == 0) {
    return;
  }
  show_preset_from((uint8_t)((position_in_preset + 1) %
                             resolved_count[current_preset]));
}

void scene_force_redraw() {
  if (current_scene_index >= 0) {
    scenes[current_scene_index]->request_redraw();
  }
}

void tick() {
  if (current_scene_index >= 0) {
    scenes[current_scene_index]->update();
  }
}

void scene_switcher_set_wifi_available(bool available) {
  if (wifi_available == available) {
    return;
  }
  wifi_available = available;

  if (scenes.empty()) {
    return;
  }

  if (available) {
    // Wi-Fi-gated scenes are back in play; if we had to fall back, return to
    // the preset's own rotation.
    if (on_fallback) {
      show_preset_from(position_in_preset);
    }
  } else if (!on_fallback && resolved_count[current_preset] > 0 &&
             !is_scene_valid(current_scene_index)) {
    // The scene we are showing just became unavailable.
    rotation_advance();
  }
}

// --- Introspection and direct selection (HTTP API / HA integration) ---

void preset_select(uint8_t index) {
  if (index < PRESET_COUNT) {
    select_preset(index);
  }
}

const char *current_scene_name() {
  if (current_scene_index < 0 || (size_t)current_scene_index >= scenes.size()) {
    return "-";
  }
  return scenes[current_scene_index]->get_scene_name();
}

bool scene_select_by_name(const char *name) {
  if (name == nullptr) {
    return false;
  }
  const int idx = scene_index_by_name(name);
  if (idx < 0) {
    return false;
  }

  // Stay inside the preset rotation when the scene belongs to it; otherwise
  // show it directly until the next rotation (fallback mode).
  bool in_preset = false;
  const uint8_t count = resolved_count[current_preset];
  for (uint8_t pos = 0; pos < count; pos++) {
    if (resolved[current_preset][pos] == idx) {
      position_in_preset = pos;
      in_preset = true;
      break;
    }
  }
  on_fallback = !in_preset;
  activate_index(idx);
  return true;
}

int scene_count() { return (int)scenes.size(); }

const char *scene_name_at(int index) {
  if (index < 0 || (size_t)index >= scenes.size()) {
    return nullptr;
  }
  return scenes[index]->get_scene_name();
}

const char *preset_name_at(uint8_t index) {
  return index < PRESET_COUNT ? presets[index].name : nullptr;
}

uint8_t preset_scene_count_at(uint8_t index) {
  return index < PRESET_COUNT ? resolved_count[index] : 0;
}

const char *preset_scene_name_at(uint8_t index, uint8_t position) {
  if (index >= PRESET_COUNT || position >= resolved_count[index]) {
    return nullptr;
  }
  return scene_name_at(resolved[index][position]);
}
