#pragma once

#include "sprites.hpp"

extern const uint8_t
    rain_animation_data_start[] asm("_binary_rain_bwb_start");
extern const uint8_t rain_animation_data_end[] asm("_binary_rain_bwb_end");

struct RainAnimation : AnimationSheet {
  RainAnimation() : AnimationSheet(rain_animation_data_start, rain_animation_data_end) {
  }
};
