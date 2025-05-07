#include "led.h"
#include "font.h"

void panel_init()
{
    pinMode(P_LATCH, OUTPUT);
    pinMode(P_CLK, OUTPUT);
    pinMode(P_DI, OUTPUT);
    pinMode(P_EN, OUTPUT);

    /* LEDC PWM on the EN/OE pin */
    ledcSetup(EN_CH, EN_FREQ, EN_RES);
    ledcAttachPin(P_EN, EN_CH);

    ledcWrite(EN_CH, MIN_BRIGHTNESS); // keep blank during init
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
    uint32_t dur = BASE_TIME;
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        ledcWrite(EN_CH, MIN_BRIGHTNESS); // 1) blank panel while shifting
        digitalWrite(P_LATCH, LOW);         //    keep LA/ LOW so outputs freeze
        shiftPlane(bit);                  // 2) clock 256 bits of this plane
        digitalWrite(P_LATCH, HIGH);        // 3) short HIGH pulse latches data
        digitalWrite(P_LATCH, LOW);
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
