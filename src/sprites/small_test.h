#pragma once

#include <Arduino.h>
#include "sprites.hpp"


// sprite size: 4x4 (4x8 total)
constexpr uint8_t spriteData[2*4] = {
		0xf0, 0xf0, 0x0f, 0x0f, //
		0xf0, 0xf0, 0xff, 0xff, //
};

// keep only ONE of the following:
constexpr TextureAtlas atlas(spriteData, 4, 4, 2);
constexpr SpriteSheet atlas(spriteData, 4, 4, 2);
constexpr FontSheet atlas(spriteData, 4, 4, 2, 32);
