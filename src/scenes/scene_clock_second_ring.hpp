#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "clock.h"
#include "sprites/thin_glyphs.hpp"
#include "scenes/helper.hpp"

// bold clock scene
class ClockSceneWithSecondHand : public Scene
{
private:
    void drawTime(uint8_t hour, uint8_t minute, uint8_t second = 0)
    {
        const uint8_t *sprite;

        panel_clear();

        // draw second hand around it
        uint8_t x, y;
        for (uint8_t i = 0; i < 60; i++)
        {
            ring_coord((i + 8) % 60, x, y); // start at 8 to have the second hand at the center top
            if (i == second)
            {
                panel_setPixel(y, x, BRIGHTNESS_3); // bright for second hand
            }
            else
            {
                panel_setPixel(y, x, BRIGHTNESS_1); // dim for other pixels
            }
        }

        // hour first digit
        thin_font.drawGlyph((hour / 10) + 48, 3, 1); // hour/10 is 0,1 or 2

        // hour second digit
        thin_font.drawGlyph((hour % 10) + 48, 9, 1); // hour % 10 is 0-9

        // minute first digit
        thin_font.drawGlyph((minute / 10) + 48, 3, 9); // minute/10 is 0-5

        // minute second digit
        thin_font.drawGlyph((minute % 10) + 48, 9, 9); // minute % 10 is 0-9

        panel_commit();
    }

public:
    void activate() override
    {
        Serial.println("Clock scene activated");
        struct tm time = time_get();
        drawTime(time.tm_hour, time.tm_min);
    }

    void update() override
    {
        static int lastSecond = -1;

        struct tm time = time_get();
        if (time.tm_sec != lastSecond)
        {
            drawTime(time.tm_hour, time.tm_min, time.tm_sec);
            ;
            lastSecond = time.tm_sec;
        }
    }
};
