#pragma once

#include "sprites.hpp"

// sprite size: 7x8 (total: 7x104)
extern const uint8_t
    rain_animation_start[] asm("_binary_rain_animation_bmp_start");
extern const uint8_t rain_animation_end[] asm("_binary_rain_animation_bmp_end");

struct RainAnimation : AnimationSheet {
  RainAnimation() : AnimationSheet(rain_animation_start, rain_animation_end) {
  }
};
