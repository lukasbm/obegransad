#include <Arduino.h>
#include "scene.h"
#include "led.h"

// A simple scene where a snake moves around the screen with a tail following
// moves once a second
// the tail becomes gradually dimmer (length 4)
// this is great for testing the LEDs
class SnakeScene : public Scene
{
private:
    unsigned long lastUpdateTime = 0;
    short headPos = 0;

    void drawSnake()
    {
        uint8_t x, y;

        Serial.printf("Snake head: %d\n", headPos);

        // head
        ring_coord((headPos + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_4);

        ring_coord((headPos - 1 + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_3);

        ring_coord((headPos - 2 + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_2);

        ring_coord((headPos - 3 + 60) % 60, x, y);
        panel_setPixel(y, x, BRIGHTNESS_1);
    }

    // pos is one of the 60 corner pixels. writes the x and y coordinates.
    // pos 0 is top left, moving clockwise
    void ring_coord(uint8_t pos, uint8_t &x, uint8_t &y)
    {
        if (pos < 16) // 16 in the top row
        {
            x = pos;
            y = 0;
        }
        else if (pos < 30) // 16 in the top row and 14 down
        {
            x = 15;
            y = pos - 15;
        }
        else if (pos < 46)
        {
            x = 45 - pos;
            y = 15;
        }
        else
        {
            x = 0;
            y = 60 - pos;
        }
    }

public:
    void activate() override
    {
        Serial.println("Snake Scene activated");
    }
    void update() override
    {
        if (millis() > lastUpdateTime + 1000)
        {
            panel_clear();
            drawSnake();
            headPos = (headPos + 1) % 60;
            lastUpdateTime = millis();
        }
    }
};
