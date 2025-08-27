#pragma once

#include "sprites.hpp"

extern const uint8_t
    moon_atlas_data_start[] asm("_binary_moon_small_bwb_start");
extern const uint8_t moon_atlas_data_end[] asm("_binary_moon_small_bwb_end");

struct MoonAtlas : TextureAtlas {
  MoonAtlas() : TextureAtlas(moon_atlas_data_start, moon_atlas_data_end) {}
};

// global instance as there is no instance state
const MoonAtlas moon_atlas;
