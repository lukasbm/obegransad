#pragma once

#include "frame.hpp"
#include "sprites/bold_glyphs.hpp"
#include "sprites/wifi.hpp"

/**
 * @brief Builders for the app's popup artwork (see popup.h).
 *
 * Kept apart from the popup module so that stays generic: this is the only
 * place that knows what our overlays look like.
 */

/**
 * @brief A single digit filling the panel — used to show the active preset.
 * The 6x7 bold glyph is drawn at 2x (12x14) and centred.
 */
inline Frame overlay_preset_number(uint8_t number) {
  Frame frame;
  frame.clear();
  font_bold.drawGlyph((char)('0' + (number % 10)), 2, 1, &frame, 2);
  return frame;
}

/**
 * @brief The Wi-Fi symbol, struck through when connectivity was lost.
 * The 10x8 sprite is centred; the "lost" variant carves a diagonal out of it so
 * the two states are unmistakable at a glance.
 */
inline Frame overlay_wifi(bool connected) {
  const uint8_t SPRITE_X = (PANEL_WIDTH - wifi_sprite.width()) / 2;
  const uint8_t SPRITE_Y = (PANEL_HEIGHT - wifi_sprite.height()) / 2;

  Frame frame;
  frame.clear();
  wifi_sprite.draw(SPRITE_X, SPRITE_Y, &frame);

  if (!connected) {
    // Bottom-left to top-right slash, with the pixel above it blanked so the
    // line stays readable where it crosses the lit part of the sprite.
    for (uint8_t i = 2; i < PANEL_WIDTH - 2; i++) {
      frame.set(PANEL_HEIGHT - 1 - i, i, PANEL_BRIGHTNESS_3);
      frame.set(PANEL_HEIGHT - 2 - i, i, PANEL_BRIGHTNESS_OFF);
    }
  }
  return frame;
}
