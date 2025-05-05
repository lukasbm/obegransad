#include "led.h"
#include "font.h"

void panel_init()
{
    pinMode(P_CLA, OUTPUT);
    pinMode(P_CLK, OUTPUT);
    pinMode(P_DI, OUTPUT);
    pinMode(P_EN, OUTPUT);
}

// Clear the Panel Buffer
void panel_clear()
{
    for (int i = 0; i < 256; i++)
    {
        panel_buf[i] = 0;
    }
}

// SCAN DISPLAY, output Bytes to Serial to display
void panel_show()
{
    uint8_t cmask = 1; // to force it to a binary image.

    panel_setBrightness(255);
    delayMicroseconds(TT);

    uint8_t w = 0;
    for (int i = 256; i > 0; i--)
    {
        digitalWrite(P_DI, cmask & panel_buf[w++]);
        digitalWrite(P_CLK, HIGH);
        digitalWrite(P_CLK, LOW);

    } // update 2024/01/01 speedup
    digitalWrite(P_CLA, HIGH);
    digitalWrite(P_CLA, LOW);
    panel_setBrightness(brightness); // re enable brightness
}

void panel_setPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if ((x < 16) && (y < 16))
    {
        panel_buf[lut[y][x]] = color;
    }
}

void panel_fillGrid(uint8_t col)
{
    for (uint8_t x = 0; x < 16; x++)
        for (uint8_t y = 0; y < 16; y++)
            panel_setPixel(x, y, col);
}

void panel_printChar(uint8_t xs, uint8_t ys, char ch)
{
    uint8_t d;

    for (uint8_t x = 0; x < 6; x++)
    {

        d = pgm_read_byte_near((ch - 32) * 6 + // Buchstabennummer (ASCII ) minus 32 da die ersten 32 Zeichen nicht im Font sind
                               x +             // jede Spalte
                               BoldGlyphs6x7); // Adress of Font

        if ((d & 1) == 1)
            panel_setPixel(x + xs, 0 + ys, 0xFF);
        else
            panel_setPixel(x + xs, 0 + ys, 0);
        if ((d & 2) == 2)
            panel_setPixel(x + xs, 1 + ys, 0xFF);
        else
            panel_setPixel(x + xs, 1 + ys, 0);
        if ((d & 4) == 4)
            panel_setPixel(x + xs, 2 + ys, 0xFF);
        else
            panel_setPixel(x + xs, 2 + ys, 0);
        if ((d & 8) == 8)
            panel_setPixel(x + xs, 3 + ys, 0xFF);
        else
            panel_setPixel(x + xs, 3 + ys, 0);
        if ((d & 16) == 16)
            panel_setPixel(x + xs, 4 + ys, 0xFF);
        else
            panel_setPixel(x + xs, 4 + ys, 0);
        if ((d & 32) == 32)
            panel_setPixel(x + xs, 5 + ys, 0xFF);
        else
            panel_setPixel(x + xs, 5 + ys, 0);
        if ((d & 64) == 64)
            panel_setPixel(x + xs, 6 + ys, 0xFF);
        else
            panel_setPixel(x + xs, 6 + ys, 0);
    }
}

void panel_setBrightness(uint8_t brightness)
{
    analogWrite(P_EN, brightness);
}
