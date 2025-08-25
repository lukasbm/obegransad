#pragma once

#include "scene.h"
#include <cstddef>

/**
 * @brief Registers a scene to the scene switcher.
 * It uses the scenes static array to store the scene and to avoid duplicates.
 * The scenes are stored in a list
 */
void register_scene(Scene *scene);

/**
 * @brief Unregisters a scene from the scene switcher.
 * It uses the scenes static name to remove the scene.
 */
void unregister_scene(Scene *scene);

/**
 * @brief Advances to the next scene.
 * It calls the activate method of the next scene and deactivates the current
 * one.
 */
void next_scene();

/**
 * @brief Goes back to the previous scene.
 * It calls the activate method of the previous scene and deactivates the
 * current one.
 */
void prev_scene();

/**
 * @brief Goes to the scene with the given index.
 */
void skipTo(size_t idx);

/**
 * @brief Updates the current scene.
 * It calls the update method of the current scene.
 */
void tick();
