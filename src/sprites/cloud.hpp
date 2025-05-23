#pragma once

#include <Arduino.h>
#include "sprites.hpp"

// img size: 9x5
constexpr uint8_t data_cloud[12] = {0x0f, 0xcf, 0x0c, 0x2c, 0xbc, 0x00, 0x1f, 0x40, 0x0f, 0x3f, 0xfc, 0x00};

constexpr SingleSprite sprite_cloud(data_cloud, 9, 5);
