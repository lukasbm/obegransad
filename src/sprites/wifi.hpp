#pragma once

#include <Arduino.h>
#include "sprites.hpp"

// image size: 10x8
static constexpr uint8_t wifi_sprite_data[20] = {0x0f, 0xff, 0x03, 0x00, 0x0c, 0xc0, 0x00, 0x30, 0x3f, 0xc0, 0x0c, 0x03, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x0f, 0x00};

struct WifiSprite : SingleSprite
{
    constexpr WifiSprite() : SingleSprite(wifi_sprite_data, 10, 8, 20) {}
};

// global instance as there is no instance state
const WifiSprite wifi_sprite;
