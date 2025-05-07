#include "led.h"
#include "font.h"

void panel_init()
{
    pinMode(P_CLA, OUTPUT);
    pinMode(P_CLK, OUTPUT);
    pinMode(P_DI, OUTPUT);
    pinMode(P_EN, OUTPUT);

    /* LEDC PWM on the EN/OE pin */
    ledcSetup(EN_CH, EN_FREQ, EN_RES);
    ledcAttachPin(P_EN, EN_CH);

    ledcWrite(EN_CH, MIN_BRIGHTNESS); // keep blank during init
}

// helper: shift one bit‑plane
inline void shiftPlane(uint8_t bit)
{
    uint8_t *p = panel_buf;
    for (uint16_t i = 0; i < 256; i++, p++)
    {
        digitalWrite(P_DI, (*p >> bit) & 1);
        digitalWrite(P_CLK, HIGH);
        digitalWrite(P_CLK, LOW);
    }
    // latch outputs
    digitalWrite(P_CLA, HIGH);
    // IMPORTANT: leave P_LATCH high, many driver ICs
    // keep outputs enabled only while LE is HIGH
    // digitalWrite(P_CLA, LOW);
}

void panel_show()
{
    uint32_t dur = BASE_TIME;
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        ledcWrite(EN_CH, MIN_BRIGHTNESS); // 1) BLANK the panel
        shiftPlane(bit);                  // 2) clock 256 bits (LA/ kept LOW)
        digitalWrite(P_CLA, HIGH);        // 3) copy SR->latch (≈ 50 ns)
        digitalWrite(P_CLA, LOW);         //    …and freeze it again
        ledcWrite(EN_CH, MAX_BRIGHTNESS); // 4) show this bit‑plane
        delayMicroseconds(dur);           // 5) keep it for weighted time
        dur <<= 1;                        // the higher the bit, the longer the time slice
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
