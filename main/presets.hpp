#pragma once

#include "sdkconfig.h"
#include <cstdint>
#include <iterator>

/**
 * @brief Scene presets — the ten rotations a short button press cycles through.
 *
 * Each preset is an ordered list of scenes plus how long each one is shown.
 * The order in the list *is* the rotation, so alternating e.g. clock and
 * weather is simply a matter of writing them alternately.
 *
 * To edit a preset: change its scene array in `preset_scenes` below, and its
 * name/dwell in the `presets` table. Scenes are named by their
 * Scene::get_scene_name() string; the names are resolved once at startup (see
 * scene_switcher_init()) and any that do not match a registered scene are
 * logged and skipped. Unused slots are OBG_PRESET_EMPTY and are skipped when
 * cycling.
 */
struct Preset {
  const char *name;
  uint32_t dwell_ms;         // how long each scene is shown; 0 = never rotate
  const char *const *scenes; // rotation, in order
  uint8_t scene_count;
};

inline constexpr uint8_t PRESET_COUNT = 10;      // numbered 0..9
inline constexpr uint8_t MAX_PRESET_SCENES = 12; // per preset

namespace preset_scenes {
inline constexpr const char *calm[] = {"Digital Clock"};
inline constexpr const char *time_and_weather[] = {
    "Digital Clock", "Current Weather", "Clock w/ seconds", "Daily Weather"};
inline constexpr const char *fun[] = {"Game of Life", "Concentric Circles",
                                      "Rotating Cube"};
inline constexpr const char *everything[] = {
    "Digital Clock",   "Current Weather", "Clock w/ seconds",
    "Weather Forecast", "Anniversary",    "Daily Weather",
    "Game of Life",     "Snake",          "Concentric Circles",
    "Rotating Cube"};
inline constexpr const char *seconds[] = {"Clock w/ seconds"};
inline constexpr const char *game_and_time[] = {"Game of Life",
                                                "Clock w/ seconds"};
} // namespace preset_scenes

// Fills in scene_count from the array, so it can never drift out of sync.
#define OBG_PRESET(name, dwell, list)                                          \
  { name, dwell, list, (uint8_t)std::size(list) }
#define OBG_PRESET_EMPTY                                                       \
  { "", 0, nullptr, 0 }

// clang-format off
inline constexpr Preset presets[PRESET_COUNT] = {
    OBG_PRESET("Calm",           0,                         preset_scenes::calm),
    OBG_PRESET("Time & Weather", 15000,                     preset_scenes::time_and_weather),
    OBG_PRESET("Fun",            30000,                     preset_scenes::fun),
    OBG_PRESET("Everything",     CONFIG_OBG_SCENE_DWELL_MS, preset_scenes::everything),
    OBG_PRESET("Seconds",        0,                         preset_scenes::seconds),
    OBG_PRESET("Game & Time",    10000,                     preset_scenes::game_and_time),
    OBG_PRESET_EMPTY,
    OBG_PRESET_EMPTY,
    OBG_PRESET_EMPTY,
    OBG_PRESET_EMPTY,
};
// clang-format on
