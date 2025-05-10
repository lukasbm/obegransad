#pragma once

#include <Arduino.h>

// NOTE: sprite are always stacked on top of each other (y-axis)

// for static sprites
struct TextureAtlas
{
    const uint8_t width;
    const uint8_t height;
    const uint8_t **data;
    const uint8_t spriteWidth;
    const uint8_t spriteHeight;
    const uint8_t spriteCount;

    TextureAtlas(const uint8_t w, const uint8_t h, const uint8_t **d, const uint8_t sw, const uint8_t sh, const uint8_t sc)
        : width(w), height(h), data(d), spriteWidth(sw), spriteHeight(sh), spriteCount(sc) {}

    unsigned short getByIndex(unsigned short index, const uint8_t *out)
    {
        if (index >= spriteCount)
        {
            return 0;
        }
        out = data[index];
        return spriteWidth * spriteHeight;
    }
};

struct FontSheet : TextureAtlas
{
    unsigned short getGlyph(const char c, const uint8_t *out)
    {
        if (c < 32 || c > 126)
        {
            return 0;
        }
        unsigned short index = c - 32;
        out = data[index];
        return spriteWidth * spriteHeight;
    }
};

// for animations
struct SpriteSheet : TextureAtlas
{
    unsigned short nextFrame(const uint8_t *out)
    {
        out = data[currFrame];
        currFrame++;
        if (currFrame >= spriteCount)
        {
            currFrame = 0;
        }
    }

    void startOver()
    {
        currFrame = 0;
    }

private:
    unsigned short currFrame = 0;
};
