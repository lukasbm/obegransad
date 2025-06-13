#include <Arduino.h>
#include "scene.h"
#include "led.h"

// a simple screensaver scene that displays fireworks
class GameOfLifeScene : public Scene
{
private:
    bool buffer[16][16];
    bool back_buffer[16][16];

    void evolve()
    {
        for (uint8_t y = 0; y < 16; ++y)
        {
            for (uint8_t x = 0; x < 16; ++x)
            {
                // Count alive neighbors
                int alive_neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dx == 0 && dy == 0)
                            continue; // Skip self
                        // torodial wrapping
                        int nx = (x + dx + 16) % 16;
                        int ny = (y + dy + 16) % 16;
                        if (buffer[ny][nx])
                            alive_neighbors++;
                    }
                }

                // Apply rules of life
                if (buffer[y][x])
                {
                    back_buffer[y][x] = (alive_neighbors == 2 || alive_neighbors == 3);
                }
                else
                {
                    back_buffer[y][x] = (alive_neighbors == 3);
                }
            }
        }

        // Swap buffers
        memcpy(buffer, back_buffer, sizeof(buffer));
    }

    void draw()
    {
        for (uint8_t y = 0; y < 16; ++y)
        {
            for (uint8_t x = 0; x < 16; ++x)
            {
                if (buffer[y][x])
                {
                    panel_setPixel(y, x, BRIGHTNESS_4);
                }
                else
                {
                    panel_setPixel(y, x, BRIGHTNESS_OFF);
                }
            }
        }
    }

public:
    void activate() override
    {
        Serial.println("Fireworks Scene activated");
        panel_clear();
        memset(buffer, 0, sizeof(buffer));  // FIXME: get initial state from settings
        memset(back_buffer, 0, sizeof(back_buffer));
    }

    void update() override
    {
        static unsigned long lastUpdateTime = 0;

        auto now = millis();
        if (now - lastUpdateTime > 500) // twice a second
        {
            evolve();
            draw();
            lastUpdateTime = now;
        }
    }
};
