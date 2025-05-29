#pragma once

#include <Arduino.h>
#include "sprites.hpp"

// sprite size: 4x4 (4x8 total)
static constexpr uint8_t data[2 * 4] = {
	0xf0, 0xf0, 0x0f, 0x0f, //
	0xf0, 0xf0, 0xff, 0xff, //
};

struct TestSprite : TextureAtlas
{
	constexpr TestSprite() : TextureAtlas(data, 4, 4, 2, 4) {}
};
