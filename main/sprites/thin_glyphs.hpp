#pragma once

#include "sprites.hpp"

extern const uint8_t
    font_thin_data_start[] asm("_binary_thin_glyphs_bwb_start");
extern const uint8_t font_thin_data_end[] asm("_binary_thin_glyphs_bwb_end");

struct ThinFont : FontSheet {
  ThinFont() : FontSheet(font_thin_data_start, font_thin_data_end, 32) {}
};

// global instance as there is no instance state
const ThinFont font_thin;
