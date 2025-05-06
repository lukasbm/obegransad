#include "led.h"
#include "font.h"

void panel_init()
{
    pinMode(P_CLA, OUTPUT);
    pinMode(P_CLK, OUTPUT);
    pinMode(P_DI, OUTPUT);
    pinMode(P_EN, OUTPUT);
    digitalWrite(P_EN, LOW); // keep panel blank during setup
}

/*
push ONE bit‑plane and display it for “dur” microseconds.
A bit plane is one part of the 256 light levels of an LED.
*/
void panel_show_bitPlane(uint8_t bit, uint16_t dur_us)
{
    // push ONE bit‑plane. DI shoves it into a shift register.
    for (uint16_t i = 0; i < 256; i++)
    {
        digitalWrite(P_DI, (panel_buf[i] >> bit) & 1);
        digitalWrite(P_CLK, HIGH);
        digitalWrite(P_CLK, LOW);
    }
    
    // latch the 256 bits
    digitalWrite(P_CLA, HIGH);
    digitalWrite(P_CLA, LOW);

    // turn on all LEDS of this bitplane according to their weight (dur_us)
    digitalWrite(P_EN, HIGH);  // turn LEDs on
    delayMicroseconds(dur_us); // time slice for this bit‑plane
    digitalWrite(P_EN, LOW);   // blank (before shifting next plane)
}

void panel_show()
{
    uint32_t dur = BASE_TIME;
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        panel_show_bitPlane(bit, dur);
        dur <<= 1; // the higher the bit, the longer the time slice
    }
}

void panel_setPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if ((x < 16) && (y < 16))
    {
        panel_buf[lut[y][x]] = color;
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

void panel_printChar(uint8_t xs, uint8_t ys, char ch)
{
    uint8_t d;

    // for (uint8_t x = 0; x < 6; x++)
    // {
    //     d = BoldGlyphs6x7[(ch - 32) * 6 + x]; // Buchstabennummer (ASCII ) minus 32 da die ersten 32 Zeichen nicht im Font sind jede Spalte

    //     if ((d & 1) == 1)
    //         panel_setPixel(x + xs, 0 + ys, 0xFF);
    //     else
    //         panel_setPixel(x + xs, 0 + ys, 0);
    //     if ((d & 2) == 2)
    //         panel_setPixel(x + xs, 1 + ys, 0xFF);
    //     else
    //         panel_setPixel(x + xs, 1 + ys, 0);
    //     if ((d & 4) == 4)
    //         panel_setPixel(x + xs, 2 + ys, 0xFF);
    //     else
    //         panel_setPixel(x + xs, 2 + ys, 0);
    //     if ((d & 8) == 8)
    //         panel_setPixel(x + xs, 3 + ys, 0xFF);
    //     else
    //         panel_setPixel(x + xs, 3 + ys, 0);
    //     if ((d & 16) == 16)
    //         panel_setPixel(x + xs, 4 + ys, 0xFF);
    //     else
    //         panel_setPixel(x + xs, 4 + ys, 0);
    //     if ((d & 32) == 32)
    //         panel_setPixel(x + xs, 5 + ys, 0xFF);
    //     else
    //         panel_setPixel(x + xs, 5 + ys, 0);
    //     if ((d & 64) == 64)
    //         panel_setPixel(x + xs, 6 + ys, 0xFF);
    //     else
    //         panel_setPixel(x + xs, 6 + ys, 0);
    // }
}

// FIXME: dont use!!!
void panel_setBrightness(uint8_t brightness)
{
    analogWrite(P_EN, brightness);
}
