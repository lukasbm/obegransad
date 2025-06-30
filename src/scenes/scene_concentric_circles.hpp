#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "scenes/helper.hpp"

// screensaver scene where concretric circles are drawn
// this is a recreation of the original (unmodified) obegransad scene.
class ConcentricCircleScene : public Scene
{
private:
    // radius of the circles
    uint8_t w1 = 0;               // outer radius of first white ring
    uint8_t w2 = 0;               // outer radius of second white ring
    const uint8_t whiteWidth = 6; // radius of the white ring to be added

    uint8_t distanceToCenter(uint8_t x, uint8_t y)
    {
        return (uint8_t)sqrtf((x - 7) * (x - 7) + (y - 7) * (y - 7));
    }

    void draw()
    {
        for (uint8_t y = 0; y < 16; y++)
        {
            for (uint8_t x = 0; x < 16; x++)
            {
                uint8_t dist = distanceToCenter(x, y);
                if (dist == w1 || dist == w2)
                {
                    // outer border (lower brightness)
                    panel_setPixel(y, x, BRIGHTNESS_2);
                }
                else if (dist == w1 - whiteWidth || dist == w2 - whiteWidth)
                {
                    // inner border (higher brightness)
                    panel_setPixel(y, x, BRIGHTNESS_2);
                }
                else if ((dist < w1 && dist > w1 - whiteWidth) || (dist < w2 && dist > w2 - whiteWidth))
                {
                    // full pixel of the white ring
                    panel_setPixel(y, x, BRIGHTNESS_3);
                }
                else
                {
                    // no pixel
                    panel_setPixel(y, x, BRIGHTNESS_OFF);
                }
            }
        }
        panel_commit();
    }

public:
    void activate() override
    {
        Serial.println("Concentric Circle scene activated");
        w2 = 15;
        w1 = w2 - 10; // 10 so they are equally spaced in the 20 radius wheel.
        draw();
    }
    void update() override
    {
        static unsigned long lastUpdateTime = 0;

        if (millis() - lastUpdateTime > 100) // 10FPS
        {
            w1 = (w1 + 1) % 20;
            w2 = (w2 + 1) % 20;
            draw();
            lastUpdateTime = millis();
        }
    }
};
