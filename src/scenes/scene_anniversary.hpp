#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "sprites/thin_glyphs.hpp"
#include "sprites/sprite_heart.hpp"
#include "config.h"

// scene to display anniversary dates with a heart animation
class AnniversaryScene : public Scene
{
private:
    HeartAnimation animation_heart;

    void drawHeart(uint8_t day, uint8_t month)
    {
        const uint8_t *sprite;

        panel_clear();

        // update animation
        sprite = animation_heart.nextFrame();
        panel_drawSprite(0, 0, sprite, animation_heart.spriteWidth, animation_heart.spriteHeight);

        // day first digit
        sprite = thin_font.getGlyph((day / 10) + 48); // day/10 is 0,1 or 2
        panel_drawSprite(0, 10, sprite, thin_font.spriteWidth, thin_font.spriteHeight);

        // day second digit
        sprite = thin_font.getGlyph((day % 10) + 48); // day % 10 is 0-9
        panel_drawSprite(4, 10, sprite, thin_font.spriteWidth, thin_font.spriteHeight);

        // month first digit
        sprite = thin_font.getGlyph((month / 10) + 48); // month/10 is 0-5
        panel_drawSprite(8, 10, sprite, thin_font.spriteWidth, thin_font.spriteHeight);

        // month second digit
        sprite = thin_font.getGlyph((month % 10) + 48); // month % 10 is 0-9
        panel_drawSprite(12, 10, sprite, thin_font.spriteWidth, thin_font.spriteHeight);
    }

public:
    void activate() override
    {
        Serial.println("Heart scene activated");
        drawHeart(gSettings.anniversary_day, gSettings.anniversary_month);
    }

    void update() override
    {
        static int lastDraw = millis();

        // update animation (4 times a second - 4 FPS)
        if (millis() - lastDraw > 250)
        {
            drawHeart(gSettings.anniversary_day, gSettings.anniversary_month);
            lastDraw = millis();
        }
    }
};
