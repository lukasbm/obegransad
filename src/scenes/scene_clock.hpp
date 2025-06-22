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
        sprite = font_bold.getGlyph((hour / 10) + 48); // hour/10 is 0,1 or 2
        panel_drawSprite(2, 0, sprite, font_bold.spriteWidth, font_bold.spriteHeight);

        // hour second digit
        sprite = font_bold.getGlyph((hour % 10) + 48); // hour % 10 is 0-9
        panel_drawSprite(9, 0, sprite, font_bold.spriteWidth, font_bold.spriteHeight);

        // minute first digit
        sprite = font_bold.getGlyph((minute / 10) + 48); // minute/10 is 0-5
        panel_drawSprite(2, 9, sprite, font_bold.spriteWidth, font_bold.spriteHeight);

        // minute second digit
        sprite = font_bold.getGlyph((minute % 10) + 48); // minute % 10 is 0-9
        panel_drawSprite(9, 9, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
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
