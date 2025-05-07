#include "led.h"
#include "font.h"

void panel_init()
{
    pinMode(P_LATCH, OUTPUT);
    pinMode(P_CLK, OUTPUT);
    pinMode(P_DI, OUTPUT);
    pinMode(P_EN, OUTPUT);
}

inline void shift_and_latch(uint8_t bit)
{
    noInterrupts();
    digitalWrite(P_LATCH, LOW); // freeze outputs
    digitalWrite(P_EN, HIGH);   // OE/ HIGH  (panel dark)

    uint16_t i = 0;
    for (; i < 256; i++)
    {
        digitalWrite(P_DI, (panel_buf[i] >> bit) & 1);
        digitalWrite(P_CLK, HIGH);
        digitalWrite(P_CLK, LOW);
    }

    digitalWrite(P_LATCH, HIGH); // transfer SR -> outputs
    delayMicroseconds(1);        // ≥ 20 ns is enough
    digitalWrite(P_LATCH, LOW);  // freeze outputs again
    interrupts();
}

void panel_setPixel(int8_t row, int8_t col, uint8_t brightness)
{
    if ((row < 16) && (row < 16))
    {
        panel_buf[lut[row][col]] = brightness;
    }
}

void panel_show()
{
    // FIXME: remove the next line
    global_brightness = (global_brightness + 1) % 256;
    Serial.println(global_brightness);

    uint32_t slice = (BASE_TIME * global_brightness) / 255;
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        shift_and_latch(bit);     // clock + latch atomically
        digitalWrite(P_EN, LOW);  // OE/ LOW  → LEDs ON
        delayMicroseconds(slice); // hold slice
        digitalWrite(P_EN, HIGH); // OE/ HIGH → LEDs OFF
        slice <<= 1;              // next bit weight
    }
}

// Clear the Panel Buffer
void panel_fill(uint8_t col)
{
    for (int i = 0; i < 256; i++)
    {
        panel_buf[i] = col;
    }
}

void panel_print(void)
{
    Serial.println("Panel buffer:");
    for (int i = 0; i < 256; i++)
    {
        if (i % 16 == 0)
        {
            Serial.println();
        }
        Serial.print(panel_buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}
