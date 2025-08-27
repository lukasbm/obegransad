#pragma once

#include "sprites.hpp"

extern const uint8_t cloud_sprite_data_start[] asm("_binary_cloud_bwb_start");
extern const uint8_t cloud_sprite_data_end[] asm("_binary_cloud_bwb_end");

struct CloudSprite : SingleSprite {
  CloudSprite()
      : SingleSprite(cloud_sprite_data_start, cloud_sprite_data_end) {}
};

// global instance as there is no instance state
const CloudSprite cloud_sprite;
