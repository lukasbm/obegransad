#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "clock.h"
#include "sprites/thin_glyphs.hpp"
#include "scenes/helper.hpp"

// bold clock scene
class ClockScene : public Scene
{
private:
    void drawTime(uint8_t hour, uint8_t minute, uint8_t second = 0)
    {
        const uint8_t *sprite;

        panel_clear();

        // draw second hand around it
        uint8_t x, y;
        for(uint8_t i = 0; i < 60; i++)
        {
            ring_coord((i + 8) % 60 , x, y);  // start at 8 to have the second hand at the center top
            if (i == second)
            {
                panel_setPixel(y, x, BRIGHTNESS_4); // bright for second hand
            }
            else
            {
                panel_setPixel(y, x, BRIGHTNESS_1); // dim for other pixels
            }
        }

        // hour first digit
        sprite = thin_font.getGlyph((hour / 10) + 48); // hour/10 is 0,1 or 2
        panel_drawSprite(3, 2, sprite, thin_font.spriteWidth, thin_font.spriteHeight);

        // hour second digit
        sprite = thin_font.getGlyph((hour % 10) + 48); // hour % 10 is 0-9
        panel_drawSprite(9, 2, sprite, thin_font.spriteWidth, thin_font.spriteHeight);

        // minute first digit
        sprite = thin_font.getGlyph((minute / 10) + 48); // minute/10 is 0-5
        panel_drawSprite(3, 8, sprite, thin_font.spriteWidth, thin_font.spriteHeight);

        // minute second digit
        sprite = thin_font.getGlyph((minute % 10) + 48); // minute % 10 is 0-9
        panel_drawSprite(9, 8, sprite, thin_font.spriteWidth, thin_font.spriteHeight);
    }

public:
    void activate() override
    {
        Serial.println("Clock scene activated");
        struct tm time = time_fetch();
        drawTime(time.tm_hour, time.tm_min);
    }

    void update() override
    {
        static int lastSecond = -1;

        struct tm time = time_fetch();
        if (time.tm_sec != lastSecond)
        {
            drawTime(time.tm_hour, time.tm_min, time.tm_sec);;
            lastSecond = time.tm_sec;
        }
    }
};
