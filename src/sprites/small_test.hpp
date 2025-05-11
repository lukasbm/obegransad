#pragma once

#include <Arduino.h>
#include "sprites.hpp"


// sprite size: 4x4 (4x8 total)
constexpr uint8_t data_test[2*4] = {
		0xf0, 0xf0, 0x0f, 0x0f, //
		0xf0, 0xf0, 0xff, 0xff, //
};

constexpr TextureAtlas sheet_test(data_test, 4, 4, 2);
