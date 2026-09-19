#pragma once

#include "frame.hpp"
#include <cstdint>

/**
 * @brief A timed fullscreen overlay.
 *
 * Deliberately independent of scenes, presets and connectivity: hand it a
 * Frame and a duration and it takes over the panel for that long. Deciding
 * that the popup wins over whatever else would be drawn is the caller's job —
 * see StateMachine::update(), which skips scene rendering while one is active.
 *
 * All functions must be called from the same task that renders (the main task).
 */

/**
 * @brief Show `frame` for `duration_ms` and draw it immediately.
 * The frame is copied, so the caller may let its own copy go out of scope.
 * Calling this while a popup is up replaces it and restarts the timer.
 */
void popup_show(const Frame &frame, uint32_t duration_ms);

/// @brief True while the popup should be on screen.
bool popup_is_active();

/// @brief Re-draw the stored frame. Call once per UI tick while active.
void popup_render();

/// @brief End the popup early.
void popup_dismiss();
