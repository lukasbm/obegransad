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
    uint32_t dur = BASE_TIME;
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        ledcWrite(EN_CH, MIN_BRIGHTNESS); // 1) blank panel while shifting
        digitalWrite(P_CLA, LOW);         //    keep LA/ LOW so outputs freeze
        shiftPlane(bit);                  // 2) clock 256 bits of this plane
        digitalWrite(P_CLA, HIGH);        // 3) short HIGH pulse latches data
        digitalWrite(P_CLA, LOW);
        ledcWrite(EN_CH, MAX_BRIGHTNESS); // 4) enable LEDs
        delayMicroseconds(dur);           // 5) time slice proportional to bit
        dur <<= 1;                        // the higher the bit, the longer the time slice
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
