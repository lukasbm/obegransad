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

// A simple timer that can be used to check if a certain interval has passed
// TODO: write an alternative that does not poll millis() every time, but instead uses IDF gptimer
// DO NOT use hw_timer for this as it only allow for 2 timers and we already use it for the panel refresh
struct RenderTimer
{
    // interval in milliseconds
    RenderTimer(uint32_t interval) : interval(interval), last(0) {}

    bool check()
    {
        static uint32_t now = millis();
        if (now - last >= interval)
        {
            last = now;
            return true; // interval has passed
        }
        return false; // interval has not passed
    }

    void reset()
    {
        last = millis(); // reset the timer to the current time so that the next check will be true immediately
    }

private:
    uint32_t interval; // interval in milliseconds
    uint32_t last;     // last time the timer was checked
};
