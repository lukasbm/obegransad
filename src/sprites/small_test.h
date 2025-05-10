#pragma once

#include <Arduino.h>
#include "sprites.hpp"

static const uint8_t spriteData[2][4] = {
    {0xf0, 0xf0, 0x0f, 0x0f}, // 4x4
    {0xf0, 0xf0, 0xff, 0xff}, // 4x4
};

TextureAtlas atlas(4, 8, (const uint8_t **)spriteData, 4, 4, 2);
