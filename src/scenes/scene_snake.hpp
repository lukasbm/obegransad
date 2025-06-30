#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "scenes/helper.hpp"

// A simple scene where a snake moves around the screen with a tail following
// moves once a second
// the tail becomes gradually dimmer (length 4)
// this is great for testing the LEDs
class SnakeScene : public Scene
{
private:
    short headPos = 0;

    void drawSnake()
    {
        uint8_t x, y;

        Serial.printf("Snake head: %d\n", headPos);

        // head
        ring_coord((headPos + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_3);

        ring_coord((headPos - 1 + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_3);

        ring_coord((headPos - 2 + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_2);

        ring_coord((headPos - 3 + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_1);

        panel_commit();
    }

public:
    void activate() override
    {
        Serial.println("Snake Scene activated");
    }
    void update() override
    {
        static unsigned long lastUpdateTime = 0;

        if (millis() > lastUpdateTime + 1000)
        {
            panel_clear();
            drawSnake();
            headPos = (headPos + 1) % 60;
            lastUpdateTime = millis();
        }
    }
};
