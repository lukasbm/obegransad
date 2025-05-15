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

        sprite = font_bold.getGlyph('1');
        panel_drawSprite(0, 0, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
        sprite = font_bold.getGlyph('9');
        panel_drawSprite(0, 8, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
        sprite = font_bold.getGlyph('A');
        panel_drawSprite(9, 0, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
        sprite = font_bold.getGlyph('Y');
        panel_drawSprite(9, 8, sprite, font_bold.spriteWidth, font_bold.spriteHeight);
    }
};
