#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "clock.h"
#include "sprites/bold_glyphs.hpp"

// bold clock scene
class ClockScene : public Scene
{
private:
    void drawTime(uint8_t hour, uint8_t minute)
    {
        const uint8_t *sprite;

        panel_clear();

        // hour first digit
        font_bold.drawGlyph((hour / 10) + 48, 2, 0); // hour/10 is 0,1 or 2

        // hour second digit
        font_bold.drawGlyph((hour % 10) + 48, 9, 0); // hour % 10 is 0-9

        // minute first digit
        font_bold.drawGlyph((minute / 10) + 48, 2, 9); // minute/10 is 0-5

        // minute second digit
        font_bold.drawGlyph((minute % 10) + 48, 9, 9); // minute % 10 is 0-9

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
        static int lastMinute = -1;

        struct tm time = time_get();
        if (time.tm_min != lastMinute)
        {
            drawTime(time.tm_hour, time.tm_min);
            lastMinute = time.tm_min;
        }
    }
};
