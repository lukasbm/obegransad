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
        animation_heart.drawNextFrame(0, 0);

        // day first digit
        thin_font.drawGlyph((day / 10) + 48, 0, 10); // day/10 is 0,1 or 2

        // day second digit
        thin_font.drawGlyph((day % 10) + 48, 4, 10); // day % 10 is 0-9

        // month first digit
        thin_font.drawGlyph((month / 10) + 48, 8, 10); // month/10 is 0-5

        // month second digit
        thin_font.drawGlyph((month % 10) + 48, 12, 10); // month % 10 is 0-9

        panel_commit();
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
        // TODO: use render timer!
        if (millis() - lastDraw > 250)
        {
            drawHeart(gSettings.anniversary_day, gSettings.anniversary_month);
            lastDraw = millis();
        }
    }
};
