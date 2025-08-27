#pragma once

#include "sprites.hpp"

extern const uint8_t wifi_sprite_data_start[] asm("_binary_wifi_bwb_start");
extern const uint8_t wifi_sprite_data_end[] asm("_binary_wifi_bwb_end");

struct WifiSprite : SingleSprite {
  WifiSprite() : SingleSprite(wifi_sprite_data_start, wifi_sprite_data_end) {}
};

// global instance as there is no instance state
const WifiSprite wifi_sprite;
