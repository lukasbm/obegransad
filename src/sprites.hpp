#pragma once

#include <Arduino.h>

// NOTE: sprite are always stacked on top of each other (y-axis)

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

    unsigned short getByIndex(unsigned short index, const uint8_t *&out)
    {
        if (index >= spriteCount)
        {
            return 0;
        }
        out = &data[index * spriteBytes];
        return spriteBytes;
    }
};

// for fonts (ASCII subset)
struct FontSheet : TextureAtlas
{
    constexpr FontSheet(const uint8_t *d, const uint8_t sw, const uint8_t sh, const uint8_t sc, const char ast)
        : TextureAtlas(d, sw, sh, sc), asciiStart(ast) {}

    unsigned short getGlyph(const char c, const uint8_t *&out) const
    {
        if (c < asciiStart || c > asciiStart + spriteCount)
        {
            out = nullptr;
            return 0;
        }
        unsigned short index = c - asciiStart;
        out = &data[index * spriteBytes];
        return spriteBytes;
    }

private:
    const char asciiStart;
};

// for animations
struct SpriteSheet : TextureAtlas
{
    constexpr SpriteSheet(const uint8_t *d, const uint8_t sw, const uint8_t sh, const uint8_t sc)
        : TextureAtlas(d, sw, sh, sc) {}

    unsigned short nextFrame(const uint8_t *&out)
    {
        out = &data[currFrame * spriteBytes];
        currFrame++;
        if (currFrame >= spriteCount)
        {
            currFrame = 0;
        }
        return spriteBytes;
    }

    void startOver()
    {
        currFrame = 0;
    }

private:
    unsigned short currFrame = 0;
};
