#pragma once

#include "sprites.hpp"

extern const uint8_t
    firework_animation_data_start[] asm("_binary_firework_bwb_start");
extern const uint8_t
    firework_animation_data_end[] asm("_binary_firework_bwb_end");

struct FireworkAnimation : AnimationSheet {
  FireworkAnimation()
      : AnimationSheet(firework_animation_data_start,
                       firework_animation_data_end) {}
};
