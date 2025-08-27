#pragma once

#include "sprites.hpp"

extern const uint8_t
    heart_animation_data_start[] asm("_binary_heart_bmp_start");
extern const uint8_t heart_animation_data_end[] asm("_binary_heart_bmp_end");

struct HeartAnimation : AnimationSheet {
  HeartAnimation()
      : AnimationSheet(heart_animation_data_start, heart_animation_data_end) {}
};
