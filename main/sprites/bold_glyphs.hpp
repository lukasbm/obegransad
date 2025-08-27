#pragma once

#include "sprites.hpp"

extern const uint8_t font_bold_data_start[] asm("_binary_bold_glyphs_bwb_start");
extern const uint8_t font_bold_data_end[] asm("_binary_bold_glyphs_bwb_end");

struct FontBold : FontSheet {
  FontBold() : FontSheet(font_bold_data_start, font_bold_data_end, 32) {}
};

// global instance as there is no instance state
const FontBold font_bold;
