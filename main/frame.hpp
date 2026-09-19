#pragma once

#include "ikea-obegransad-panel.h"
#include <cstdint>
#include <cstring>

/**
 * @brief An off-screen 16x16 screenbuffer.
 *
 * The panel driver owns a private framebuffer that is only reachable through
 * panel_setPixel(); a Frame is the exchangeable counterpart — a plain value type
 * modules can fill in and hand to each other (see popup.h). Pixels hold logical
 * Brightness values; the panel's gamma/level mapping is applied by present().
 *
 * 256 bytes, no allocation. Cheap to copy, but not free — prefer passing by
 * const reference.
 */
struct Frame {
  uint8_t px[PANEL_HEIGHT][PANEL_WIDTH];

  void clear(uint8_t brightness = PANEL_BRIGHTNESS_OFF) {
    memset(px, brightness, sizeof(px));
  }

  void set(uint8_t row, uint8_t col, uint8_t brightness) {
    if (row >= PANEL_HEIGHT || col >= PANEL_WIDTH) {
      return;
    }
    px[row][col] = brightness;
  }

  uint8_t get(uint8_t row, uint8_t col) const {
    if (row >= PANEL_HEIGHT || col >= PANEL_WIDTH) {
      return PANEL_BRIGHTNESS_OFF;
    }
    return px[row][col];
  }

  /**
   * @brief Push the whole buffer to the panel and commit it.
   * Goes through panel_setPixel() so a Frame renders identically to code that
   * draws on the panel directly.
   */
  void present() const {
    for (uint8_t row = 0; row < PANEL_HEIGHT; row++) {
      for (uint8_t col = 0; col < PANEL_WIDTH; col++) {
        panel_setPixel(row, col, px[row][col]);
      }
    }
    panel_commit();
  }
};
