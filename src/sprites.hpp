#pragma once

#include <Arduino.h>
#include "led.h"

// NOTE: sprite are always stacked on top of each other (y-axis)

// draws a sprite starting at the top left corner (tlX, tlY)
// It is also possible to draw sprites that are larger than the panel or (partially) out of bounds, but they will be clipped.
void drawSprite(int8_t tlX, int8_t tlY, const uint8_t *data, uint8_t width, uint8_t height)
{
    // Iterate over each pixel of the sprite
    for (uint8_t y = 0; y < height; y++)
    {
        for (uint8_t x = 0; x < width; x++)
        {
            // Calculate the target coordinates on the panel
            int16_t targetX = tlX + x;
            int16_t targetY = tlY + y;

            // Clip the sprite, only draw pixels that are on the panel
            if (targetX >= 0 && targetX < COLS && targetY >= 0 && targetY < ROWS)
            {
                // Sprites are packed with 4 pixels (2 bits each) per byte.
                size_t pixel_index = y * width + x;
                size_t byte_index = pixel_index / 4;
                uint8_t bit_shift = 6 - (pixel_index % 4) * 2; // 6, 4, 2, 0

                // Extract the 2-bit brightness value for the current pixel
                uint8_t pixel_brightness = (data[byte_index] >> bit_shift) & 0b11;

                panel_setPixel(targetY, targetX, static_cast<Brightness>(pixel_brightness));
            }
        }
    }
}

// for a single sprite (image)
struct SingleSprite
{
    const uint8_t *data;
    const uint8_t width;
    const uint8_t height;
    const size_t bytes;

    constexpr SingleSprite(const uint8_t *d, const uint8_t w, const uint8_t h, const size_t spriteBytes)
        : data(d), width(w), height(h), bytes(spriteBytes) {}

    void draw(const uint8_t tlX, const uint8_t tlY) const
    {
        // draw the sprite at the top-left corner (tlX, tlY)
        drawSprite(tlX, tlY, data, width, height);
    }
};

// for many static sprites
struct TextureAtlas
{
    const uint8_t *data;
    const uint8_t spriteWidth;
    const uint8_t spriteHeight;
    const size_t spriteBytes;
    const size_t spriteCount;

    constexpr TextureAtlas(const uint8_t *data, const uint8_t spriteWidth, const uint8_t spriteHeight, const size_t spriteCount, const size_t spriteBytes)
        : data(data), spriteWidth(spriteWidth), spriteHeight(spriteHeight), spriteBytes(spriteBytes), spriteCount(spriteCount) {}

    const uint8_t *getByIndex(unsigned short index) const
    {
        if (index >= spriteCount)
        {
            return nullptr;
        }
        return &data[index * spriteBytes];
    }

    void drawByIndex(const unsigned short index, const uint8_t tlX, const uint8_t tlY) const
    {
        const uint8_t *spriteData = getByIndex(index);
        if (spriteData)
        {
            drawSprite(tlX, tlY, spriteData, spriteWidth, spriteHeight);
        }
    }
};

// for fonts (ASCII subset)
struct FontSheet : TextureAtlas
{
    const char asciiStart;

    constexpr FontSheet(const uint8_t *data, const uint8_t spriteWidth, const uint8_t spriteHeight, const uint8_t spriteCount, const size_t spriteBytes, const char asciiStart)
        : TextureAtlas(data, spriteWidth, spriteHeight, spriteCount, spriteBytes), asciiStart(asciiStart) {}

    const uint8_t *getGlyph(const char c) const
    {
        if (c < asciiStart || c > asciiStart + spriteCount)
        {
            return nullptr;
        }
        unsigned short index = c - asciiStart;
        return getByIndex(index);
    }

    void drawGlyph(const char c, const uint8_t tlX, const uint8_t tlY) const
    {
        const uint8_t *glyphData = getGlyph(c);
        if (glyphData)
        {
            drawSprite(tlX, tlY, glyphData, spriteWidth, spriteHeight);
        }
    }
};

// for animations
struct AnimationSheet : TextureAtlas
{
    constexpr AnimationSheet(const uint8_t *data, const uint8_t spriteWidth, const uint8_t spriteHeight, const uint8_t spriteCount, const uint8_t spriteBytes)
        : TextureAtlas(data, spriteWidth, spriteHeight, spriteCount, spriteBytes) {}

    const uint8_t *nextFrame() const
    {
        static unsigned short currFrame = 0;

        if (currFrame >= spriteCount)
        {
            currFrame = 0;
        }
        return getByIndex(currFrame++);
    }

    void drawNextFrame(const uint8_t tlX, const uint8_t tlY) const
    {
        const uint8_t *frameData = nextFrame();
        if (frameData)
        {
            drawSprite(tlX, tlY, frameData, spriteWidth, spriteHeight);
        }
    }
};

// Helper to pack four 2-bit brightness values into a single byte.
// The order is from most significant to least significant pixel in the byte.
constexpr uint8_t pack_pixels(Brightness p1, Brightness p2, Brightness p3, Brightness p4)
{
    return (static_cast<uint8_t>(p1) << 6) |
           (static_cast<uint8_t>(p2) << 4) |
           (static_cast<uint8_t>(p3) << 2) |
           (static_cast<uint8_t>(p4));
}
