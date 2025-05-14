#include <Arduino.h>
#include "scene.h"
#include "led.h"

class BrightnessScene : public Scene
{
public:
    void activate() override
    {
        Serial.println("Brightness scene activated");
        for (uint8_t y = 0; y < 16; y++)
            for (uint8_t x = 0; x < 16; x++)
                panel_setPixel(y, x, y * 16 + x);
        Serial.println("Brightness scene activated");
        panel_print();
    }
};
