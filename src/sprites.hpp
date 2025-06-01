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

    void draw(const uint8_t tlX, const uint8_t tlY) const
    {
        // draw the sprite at the top-left corner (tlX, tlY)
        panel_drawSprite(tlX, tlY, data, width, height);
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
            panel_drawSprite(tlX, tlY, spriteData, spriteWidth, spriteHeight);
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
            panel_drawSprite(tlX, tlY, glyphData, spriteWidth, spriteHeight);
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
            panel_drawSprite(tlX, tlY, frameData, spriteWidth, spriteHeight);
        }
    }
};
