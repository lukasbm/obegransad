#pragma once

#include <Arduino.h>
#include "sprites.hpp"

// sprite size: 4x4 (total: 4x8)
static constexpr uint8_t test_atlas_data[2 * 4] = {
    0xf0, 0xf0, 0x0f, 0x0f, //
    0xf0, 0xf0, 0xff, 0xff, //
};

struct TestAtlas : TextureAtlas
{
    constexpr TestAtlas() : TextureAtlas(test_atlas_data, 4, 4, 2, 4) {}
};

// global instance as there is no instance state
const TestAtlas test_atlas;
