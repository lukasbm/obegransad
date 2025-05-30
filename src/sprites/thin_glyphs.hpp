#pragma once

#include <Arduino.h>
#include "sprites.hpp"

// sprite size: 4x6 (total: 4x390)
static constexpr uint8_t thin_font_data[65 * 6] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //
    0x34, 0x34, 0x34, 0x34, 0x00, 0x34, //
    0x33, 0x33, 0x00, 0x00, 0x00, 0x00, //
    0xcc, 0xcc, 0xff, 0xcc, 0xff, 0xcc, //
    0x33, 0xcc, 0x33, 0xcc, 0x33, 0xcc, //
    0xc3, 0x07, 0x1c, 0x70, 0xc0, 0xc3, //
    0x3c, 0xcf, 0xf0, 0xcc, 0xf3, 0x3d, //
    0x0c, 0x0c, 0x00, 0x00, 0x00, 0x00, //
    0x03, 0x3c, 0xd0, 0xc0, 0x3c, 0x03, //
    0xc0, 0x3c, 0x03, 0x07, 0x3c, 0xc0, //
    0x3c, 0xc3, 0x1c, 0x34, 0xc3, 0x3c, //
    0x00, 0x00, 0x0c, 0x3f, 0x0c, 0x00, //
    0x00, 0x00, 0x00, 0x00, 0x1c, 0x70, //
    0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, //
    0x00, 0x00, 0x00, 0x00, 0x3c, 0x3c, //
    0x03, 0x03, 0x0c, 0x30, 0xc0, 0xc0, //
    0x3c, 0xd3, 0xf3, 0xcf, 0xc7, 0x3c, //
    0x0c, 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, //
    0x3c, 0xc3, 0x03, 0x0c, 0x30, 0xff, //
    0xfc, 0x03, 0x5f, 0xfc, 0x03, 0xfc, //
    0x0f, 0x33, 0xc3, 0xff, 0x03, 0x03, //
    0xff, 0xc0, 0xfc, 0x03, 0x43, 0xfc, //
    0x3d, 0xd0, 0xfc, 0xd7, 0xc3, 0x3c, //
    0xff, 0x03, 0x0c, 0x30, 0xc0, 0xc0, //
    0x3c, 0xc3, 0x77, 0xfd, 0xd3, 0x3c, //
    0x3c, 0xc3, 0xd7, 0x3f, 0x07, 0x7c, //
    0x00, 0x00, 0x0c, 0x00, 0x0c, 0x00, //
    0x00, 0x00, 0x0c, 0x00, 0x0c, 0x1c, //
    0x00, 0x03, 0x0c, 0x30, 0x0c, 0x03, //
    0x00, 0x00, 0x3f, 0x00, 0x3f, 0x00, //
    0x00, 0x30, 0x0c, 0x03, 0x0c, 0x30, //
    0x3c, 0xc3, 0x03, 0x3c, 0x10, 0x30, //
    0x3c, 0xd7, 0xc3, 0xcc, 0xd1, 0x3c, //
    0x3c, 0xc3, 0xc3, 0xff, 0xc3, 0xc3, //
    0xfc, 0xc3, 0xdc, 0xcc, 0xc3, 0xfc, //
    0x1f, 0x70, 0xc0, 0xc0, 0x70, 0x1f, //
    0xfc, 0xc7, 0xc3, 0xc3, 0xc7, 0xfc, //
    0xff, 0xc0, 0xd5, 0xff, 0xc0, 0xff, //
    0xff, 0xc0, 0xfc, 0xc0, 0xc0, 0xc0, //
    0x3f, 0xd0, 0xc0, 0xcf, 0xc3, 0x3c, //
    0xc3, 0xc3, 0xd7, 0xff, 0xc3, 0xc3, //
    0x34, 0x34, 0x34, 0x34, 0x34, 0x34, //
    0x7f, 0x03, 0x03, 0x03, 0xd7, 0x3c, //
    0xc3, 0xcc, 0xf0, 0xf0, 0xcc, 0xc3, //
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, //
    0xc3, 0xff, 0xf7, 0xc3, 0xc3, 0xc3, //
    0xc3, 0xf3, 0xf7, 0xdf, 0xc3, 0xc3, //
    0x3c, 0xc3, 0xc3, 0xc3, 0xc3, 0x3c, //
    0xfc, 0xc3, 0xc3, 0xfc, 0xc0, 0xc0, //
    0x3c, 0xc3, 0xc3, 0xc3, 0x3c, 0x03, //
    0xfc, 0xc3, 0xc3, 0xf4, 0xcc, 0xc3, //
    0x3f, 0xc0, 0xd0, 0x3d, 0x03, 0xfc, //
    0xff, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, //
    0xc3, 0xc3, 0xc3, 0xc3, 0xd7, 0x3c, //
    0xc3, 0xc3, 0xc3, 0xd7, 0x3c, 0x3c, //
    0xc3, 0xc3, 0xc3, 0xdf, 0xff, 0xc3, //
    0xc3, 0xc3, 0x4c, 0x31, 0xc3, 0xc3, //
    0xc3, 0xc3, 0xf3, 0x1c, 0x0c, 0x0c, //
    0xff, 0x03, 0x0c, 0x30, 0xc0, 0xff, //
    0xc3, 0x3c, 0xc3, 0xff, 0xc3, 0xc3, //
    0xc3, 0x3c, 0xc3, 0xc3, 0xc3, 0x3c, //
    0xc3, 0x00, 0xc3, 0xc3, 0xc3, 0x3c, //
    0x00, 0x3c, 0xc3, 0x00, 0x00, 0x00, //
    0x00, 0x00, 0x00, 0x00, 0x00, 0xff, //
    0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, //
};

struct ThinFont : FontSheet
{
    constexpr ThinFont() : FontSheet(thin_font_data, 4, 6, 65, 6, 32) {}
};

// global instance as there is no instance state
const ThinFont thin_font;
