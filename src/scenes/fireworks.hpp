#include <Arduino.h>
#include "scene.h"
#include "led.h"
#include "sprites/firework.hpp"

// TODO: a simple screensaver scene that displays fireworks
class FireworksScene : public Scene
{
private:
    void draw()
    {
        panel_clear();
        panel_commit();
    }
};
