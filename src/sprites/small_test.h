#pragma once

#include <Arduino.h>
#include "sprites.hpp"

// 4x8 sprite
static const uint8_t smallSprite[2][4] = {
    {0xf0, 0xf0, 0x0f, 0x0f},
    {0xf0, 0xf0, 0xff, 0xff},
};

TextureAtlas atlas(4, 8, (const uint8_t **)smallSprite, 4, 4, 2);
