#pragma once

#include "sprites.hpp"

extern const uint8_t sun_animation_data_start[] asm("_binary_sun_bwb_start");
extern const uint8_t sun_animation_data_end[] asm("_binary_sun_bwb_end");

struct SunAnimation : AnimationSheet {
  SunAnimation()
      : AnimationSheet(sun_animation_data_start, sun_animation_data_end) {}
};
