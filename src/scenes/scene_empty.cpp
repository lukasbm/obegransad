#include <Arduino.h>
#include "scene.h"
#include "led.h"

class EmptyScene : public Scene
{
public:
    void activate() override
    {
        panel_clear();
        Serial.println("Empty scene activated");
    }
};
