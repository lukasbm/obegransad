#pragma once

#include <Arduino.h>

// NOTE: sprite are always stacked on top of each other (y-axis)

struct SingleSprite
{
    const uint8_t *data;
    const uint8_t width;
    const uint8_t height;
    const size_t bytes;

    constexpr SingleSprite(const uint8_t *d, const uint8_t w, const uint8_t h)
        : data(d), width(w), height(h), bytes(sizeof(d)) {} // bytes((h * w) / 4) {}
};

// for many static sprites
struct TextureAtlas
{
    const uint8_t *data;
    const uint8_t spriteWidth;
    const uint8_t spriteHeight;
    const size_t spriteBytes;
    const size_t spriteCount;

    constexpr TextureAtlas(const uint8_t *d, const uint8_t sw, const uint8_t sh, const size_t sc)
        : data(d), spriteWidth(sw), spriteHeight(sh), spriteBytes((sh * sw) / 4), spriteCount(sc) {}

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
    constexpr FontSheet(const uint8_t *d, const uint8_t sw, const uint8_t sh, const uint8_t sc, const char ast)
        : TextureAtlas(d, sw, sh, sc), asciiStart(ast) {}

    const uint8_t *getGlyph(const char c) const
    {
        if (c < asciiStart || c > asciiStart + spriteCount)
        {
            return nullptr;
        }
        unsigned short index = c - asciiStart;
        return getByIndex(index);
    }

private:
    const char asciiStart;
};

// for animations
struct SpriteSheet : TextureAtlas
{
    constexpr SpriteSheet(const uint8_t *d, const uint8_t sw, const uint8_t sh, const uint8_t sc)
        : TextureAtlas(d, sw, sh, sc) {}

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
