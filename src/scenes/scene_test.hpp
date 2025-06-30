#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "sprites/bold_glyphs.hpp"

class SpriteTestScene : public Scene
{
public:
    void activate() override
    {
        uint8_t const *sprite;

        Serial.println("Sprite test scene activated");
        panel_clear();

        font_bold.drawGlyph('1', 0, 0);
        font_bold.drawGlyph('9', 0, 8);
        font_bold.drawGlyph('A', 9, 0);
        font_bold.drawGlyph('Y', 9, 8);

        panel_commit();
    }
};
