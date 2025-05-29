#pragma once

#include <Arduino.h>
#include <stdint.h>

// pos is one of the 60 corner pixels. writes the x and y coordinates.
// pos 0 is top left, moving clockwise
void ring_coord(uint8_t pos, uint8_t &x, uint8_t &y)
{
    if (pos < 16) // 16 in the top row
    {
        x = pos;
        y = 0;
    }
    else if (pos < 30) // 16 in the top row and 14 down
    {
        x = 15;
        y = pos - 15;
    }
    else if (pos < 46)
    {
        x = 45 - pos;
        y = 15;
    }
    else
    {
        x = 0;
        y = 60 - pos;
    }
}
