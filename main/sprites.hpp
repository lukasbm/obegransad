#pragma once

#include "frame.hpp"
#include "ikea-obegransad-panel.h"
#include <cstddef>
#include <cstdint>
#include <stdint.h>

/**
 * @brief This module uses a custom image file format for ESP
 * embedding.
 * It is specifically a black and white image with 8 bit color depth.
 * It does not use any compression or pallete, hence it can be streamed after
 * reading the header.
 * @note imgbwb stands for "image black and white bitmap"
 * @note sprite are always stacked on top of each other (y-axis)
 */
struct bwb_header_t {
  uint8_t w; // Image/Sprite width in pixels
  uint8_t h; // Image/Sprite height in pixels
} __attribute__((packed));

inline bwb_header_t bwb_read_header(const uint8_t *data_start) {
  if (!data_start) {
    return {0, 0}; // Return an empty header if data is null
  }
  bwb_header_t header;
  header.w = data_start[0];
  header.h = data_start[1];
  return header;
}

/**
 * @brief Draws a sprite on the LED panel, or into an off-screen Frame
 * @param tlX Top-left X coordinate (0-15)
 * @param tlY Top-left Y coordinate (0-15)
 * @param data Pointer to sprite data (one byte per pixel)
 * @param width Width of the sprite in pixels
 * @param height Height of the sprite in pixels
 * @param target Frame to draw into, or nullptr to draw straight to the panel
 * @param scale Integer upscaling factor; each source pixel becomes a
 *        scale x scale block (1 = original size)
 *
 * This function draws a sprite at the specified top-left corner (tlX, tlY).
 *
 * @note It is also possible to draw sprites that are larger than the panel or
 * (partially) out of bounds, but they will be clipped.
 */
inline void drawSprite(int8_t tlX, int8_t tlY, const uint8_t *data,
                       uint8_t width, uint8_t height, Frame *target = nullptr,
                       uint8_t scale = 1) {
  if (scale == 0) {
    return;
  }

  // Iterate over each pixel of the sprite
  for (uint8_t y = 0; y < height; y++) {
    for (uint8_t x = 0; x < width; x++) {
      const uint8_t pixel_brightness = data[y * width + x];

      // A scaled pixel covers a scale x scale block on the target
      for (uint8_t sy = 0; sy < scale; sy++) {
        for (uint8_t sx = 0; sx < scale; sx++) {
          const int16_t targetX = tlX + x * scale + sx;
          const int16_t targetY = tlY + y * scale + sy;

          // Clip the sprite, only draw pixels that are on the panel
          if (targetX < 0 || targetX >= PANEL_WIDTH || targetY < 0 ||
              targetY >= PANEL_HEIGHT) {
            continue; // Skip out-of-bounds pixels
          }

          if (target) {
            target->set((uint8_t)targetY, (uint8_t)targetX, pixel_brightness);
          } else {
            panel_setPixel((uint8_t)targetY, (uint8_t)targetX,
                           pixel_brightness);
          }
        }
      }
    }
  }
}

struct Sprite {
protected:
  const uint8_t *data_start;
  const uint8_t *data_end;
  const size_t bytes; // in bytes
  const bwb_header_t header;

public:
  Sprite(const uint8_t *start, const uint8_t *end)
      : data_start(start + sizeof(bwb_header_t)), data_end(end),
        bytes(end - start - sizeof(bwb_header_t)),
        header(bwb_read_header(start)) {}
};

// for a single sprite (image)
struct SingleSprite : Sprite {

  SingleSprite(const uint8_t *start, const uint8_t *end) : Sprite(start, end) {}

  /**
   * @brief Draws the sprite at the specified top-left corner (tlX, tlY)
   */
  void draw(const uint8_t tlX, const uint8_t tlY, Frame *target = nullptr,
            uint8_t scale = 1) const {
    drawSprite(tlX, tlY, data_start, header.w, header.h, target, scale);
  }
};

// for many static sprites
struct TextureAtlas : Sprite {
  const size_t sprite_size;  // in bytes
  const size_t sprite_count; // number of sprites in the atlas

  TextureAtlas(const uint8_t *start, const uint8_t *end)
      : Sprite(start, end), sprite_size(header.h * header.w),
        sprite_count(bytes / sprite_size) {}

  /**
   * @brief get the sprite data by index
   */
  const uint8_t *getByIndex(unsigned short index) const {
    if (index >= sprite_count) {
      return nullptr;
    }
    return &data_start[index * sprite_size];
  }

  /**
   * @brief directly draw the sprite by index at the specified top-left corner
   */
  void drawByIndex(const unsigned short index, const uint8_t tlX,
                   const uint8_t tlY, Frame *target = nullptr,
                   uint8_t scale = 1) const {
    const uint8_t *spriteData = getByIndex(index);
    if (spriteData) {
      drawSprite(tlX, tlY, spriteData, header.w, header.h, target, scale);
    }
  }
};

// for fonts (ASCII subset)
struct FontSheet : TextureAtlas {
  const char ascii_start;

  FontSheet(const uint8_t *start, const uint8_t *end, const char asciiStart)
      : TextureAtlas(start, end), ascii_start(asciiStart) {}

  const uint8_t *getGlyph(const char c) const {
    if (c < ascii_start || c > ascii_start + sprite_count) {
      return nullptr;
    }
    unsigned short index = c - ascii_start;
    return getByIndex(index);
  }

  void drawGlyph(const char c, const uint8_t tlX, const uint8_t tlY,
                 Frame *target = nullptr, uint8_t scale = 1) const {
    const uint8_t *glyphData = getGlyph(c);
    if (glyphData) {
      drawSprite(tlX, tlY, glyphData, header.w, header.h, target, scale);
    }
  }
};

// for animations
struct AnimationSheet : TextureAtlas {
  AnimationSheet(const uint8_t *start, const uint8_t *end)
      : TextureAtlas(start, end) {}

  const uint8_t *nextFrame() const {
    static unsigned short currFrame = 0;

    if (currFrame >= sprite_count) {
      currFrame = 0;
    }
    return getByIndex(currFrame++);
  }

  void drawNextFrame(const uint8_t tlX, const uint8_t tlY) const {
    const uint8_t *frameData = nextFrame();
    if (frameData) {
      drawSprite(tlX, tlY, frameData, header.w, header.h);
    }
  }
};
