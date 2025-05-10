#pragma once

#include <Arduino.h>
#include "sprites.hpp"

// 4x8 px sprite
constexpr uint8_t smallSprite[] = {
    0xf0, 0xf0, 0x0f, 0x0f, //
    0xf0, 0xf0, 0xff, 0xff, //
};

constexpr SpriteSheet smallSheet(smallSprite, 4, 4, 2);
