#pragma once

#include <Arduino.h>
#include "sprites.hpp"

// img size: 9x5
static constexpr uint8_t data[12] = {0x0f, 0xcf, 0x0c, 0x2c, 0xbc, 0x00, 0x1f, 0x40, 0x0f, 0x3f, 0xfc, 0x00};

struct SpriteCloud : SingleSprite
{
    constexpr SpriteCloud() : SingleSprite(data, 9, 5) {}
};
