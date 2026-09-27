#pragma once

#include "scene.h"
#include <cstddef>
#include <cstdint>

/**
 * @brief Registers a scene, appending it to the registry in display order.
 * Presets refer to scenes by name, so the registry order only matters for the
 * Wi-Fi fallback (the first non-Wi-Fi scene is used).
 */
void register_scene(Scene *scene);

/**
 * @brief Resolves the preset table (see presets.hpp) against the registered
 * scenes and activates preset 0. Call once, after register_all_scenes().
 */
void scene_switcher_init();

/// @brief Index of the active preset (0..PRESET_COUNT-1).
uint8_t preset_current();

/**
 * @brief Selects a preset by index (0..PRESET_COUNT-1), activating its first
 * valid scene. Out-of-range indices are ignored.
 */
void preset_select(uint8_t index);

/**
 * @brief Selects the next/previous non-empty preset, wrapping around, and
 * activates its first valid scene.
 */
void preset_next();
void preset_prev();

/**
 * @brief How long each scene of the active preset is shown.
 * 0 means the preset does not rotate at all.
 */
uint32_t preset_dwell_ms();

/**
 * @brief Advances to the next scene within the active preset, skipping scenes
 * that need Wi-Fi while it is unavailable.
 */
void rotation_advance();

/**
 * @brief Makes the current scene draw a frame on the next tick(), even if its
 * target FPS or internal change detection would have skipped it. Used after an
 * overlay covered the panel.
 */
void scene_force_redraw();

/**
 * @brief Updates the current scene.
 * It calls the update method of the current scene.
 */
void tick();

/**
 * @brief Sets the wifi availability status for scene filtering.
 * If wifi is lost and the current scene requires it, it switches to the next valid scene.
 */
void scene_switcher_set_wifi_available(bool available);

// --- Introspection and direct selection (HTTP API / HA integration) ---

/// @brief Name of the currently displayed scene, or "-" when none is active.
const char *current_scene_name();

/**
 * @brief Activates a registered scene by name (external control).
 *
 * If the scene is part of the active preset, the preset rotation continues
 * from it; otherwise it is shown directly until the next rotation.
 * Returns false when the name is unknown.
 */
bool scene_select_by_name(const char *name);

/// @brief Number of registered scenes.
int scene_count();

/// @brief Name of the scene at `index`, or nullptr when out of range.
const char *scene_name_at(int index);

/// @brief Name of preset `index`, or nullptr when out of range.
const char *preset_name_at(uint8_t index);

/// @brief Number of scenes resolved into preset `index` (0 when out of range).
uint8_t preset_scene_count_at(uint8_t index);

/// @brief Name of position `position` in preset `index`, or nullptr.
const char *preset_scene_name_at(uint8_t index, uint8_t position);
