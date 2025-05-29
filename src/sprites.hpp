#pragma once

#include <Arduino.h>

// NOTE: sprite are always stacked on top of each other (y-axis)

// for a single sprite (image)
struct SingleSprite
{
    const uint8_t *data;
    const uint8_t width;
    const uint8_t height;
    const size_t bytes;

    constexpr SingleSprite(const uint8_t *d, const uint8_t w, const uint8_t h, const size_t spriteBytes)
        : data(d), width(w), height(h), bytes(spriteBytes) {}
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
};
