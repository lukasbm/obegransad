#include "led.h"

uint8_t gBright = BRIGHTNESS_4; // global brightness (0-255)

void panel_init()
{
    pinMode(P_LATCH, OUTPUT);
    pinMode(P_CLK, OUTPUT);
    pinMode(P_DI, OUTPUT);
    pinMode(P_OE, OUTPUT);

    digitalWrite(P_LATCH, LOW); // outputs frozen
    digitalWrite(P_OE, HIGH);   // panel blank
    digitalWrite(P_CLK, LOW);   // idle state
    digitalWrite(P_DI, LOW);    // idle state
}

inline void shift_and_latch(uint8_t bit)
{
    noInterrupts();             // interrupts will throw off the timing
    digitalWrite(P_LATCH, LOW); // freeze outputs
    digitalWrite(P_OE, HIGH);   // OE/ HIGH (panel dark / blank)

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
    interrupts();                // re-enable interrupts
}

void panel_setPixel(int8_t row, int8_t col, uint8_t brightness)
{
    if ((row < 16) && (col < 16))
    {
        noInterrupts();
        panel_buf[lut[row][col]] = brightness;
        interrupts();
    }
}

void panel_show()
{
    uint32_t slice = (BASE_TIME * gBright) / 255;
    for (uint8_t bit = 0; bit < 8; bit++) // color delineation is bad on 4 bit!
    {
        shift_and_latch(bit);     // clock + latch atomically
        digitalWrite(P_OE, LOW);  // OE/ LOW  → LEDs ON
        delayMicroseconds(slice); // hold slice
        digitalWrite(P_OE, HIGH); // OE/ HIGH → LEDs OFF
        slice <<= 1;              // next bit weight
    }
}

void panel_hold()
{
    digitalWrite(P_OE, LOW); // OE/ LOW  → LEDs ON
}

// Clear the Panel Buffer
void panel_fill(uint8_t col)
{
    noInterrupts();
    for (int i = 0; i < 256; i++)
    {
        panel_buf[i] = col;
    }
    interrupts();
}

void panel_drawSprite(int8_t tlX, int8_t tlY, const uint8_t *data, uint8_t width, uint8_t height)
{
    uint8_t *p = const_cast<uint8_t *>(data);
    uint8_t i = 0;
    // move over entire sprite (2 bits per pixel)
    for (uint8_t y = 0; y < height; y++)
    {
        for (uint8_t x = 0; x < width; x++)
        {
            // move to next byte
            if (i >= 4)
            {
                p++;
                i = 0;
            }
            // get the pixel value (2 bits per pixel) - they are big endian.
            // draw it (if not out of bounds)
            if ((tlX + x >= 0) && (tlX + x < 16) && (tlY + y >= 0) && (tlY + y < 16))
            {
                // set pixel in the panel buffer
                uint8_t pixel = (*p >> (6 - i * 2)) & mask;
                panel_setPixel(tlY + y, tlX + x, colorMap[pixel]);
            }
            i++;
        }
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
