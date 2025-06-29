#pragma once

#include <Arduino.h>

/*
The panel works as follows:
- Init SPI DMA (~0.5 us)
- Use SPI DMA to transfer bit plane (~10 us)
- Latch pulse (< 0.1 us)
- Start RMT output for timed OE hold (< 0.5 us)
*/

#define P_LATCH D1 // LA/ (active‑low latch); STCP LATCH
#define P_CLK D2   // shift clock; SPI-SCK
#define P_DI D3    // serial data into SCT2024 SDI; SPI-MOSI
#define P_OE D4    // OE/ (active‑low output enable)

#define SPI_HOST SPI2_HOST
#define SPI_HZ 20000000    // 20 MHz -> 50 ns per Bit (the SCT2024 can handle up to 25 MHz)
#define BIT_COUNT 256      // bits per plane
#define FRAME_TIME_US 2500 // 2.5 ms per frame -> 400 Hz refresh rate

// manually set the additive timing windows for the 3 planes (should appear linear in brightness to the eye)
#define PLANE0_ON_US 32  // 10 %
#define PLANE1_ON_US 80  // 25 %
#define PLANE2_ON_US 320 // 100 %
#define FRAME_US PLANE2_ON_US

#define ROWS 16 // number of rows
#define COLS 16 // number of columns

enum Brightness
{
    BRIGHTNESS_OFF,
    BRIGHTNESS_1,
    BRIGHTNESS_2,
    BRIGHTNESS_3,
};

// we have a 2 bit color depth (00 = off, 01 = low, 10 = medium, 11 = high)
static const uint8_t mask = 0b11;

// LUT For OBEGRÄNSAD (they are wired weirdly)
static const int lut[16][16] = {
    {23, 22, 21, 20, 19, 18, 17, 16, 7, 6, 5, 4, 3, 2, 1, 0},
    {24, 25, 26, 27, 28, 29, 30, 31, 8, 9, 10, 11, 12, 13, 14, 15},
    {39, 38, 37, 36, 35, 34, 33, 32, 55, 54, 53, 52, 51, 50, 49, 48},
    {40, 41, 42, 43, 44, 45, 46, 47, 56, 57, 58, 59, 60, 61, 62, 63},
    {87, 86, 85, 84, 83, 82, 81, 80, 71, 70, 69, 68, 67, 66, 65, 64},
    {88, 89, 90, 91, 92, 93, 94, 95, 72, 73, 74, 75, 76, 77, 78, 79},
    {103, 102, 101, 100, 99, 98, 97, 96, 119, 118, 117, 116, 115, 114, 113, 112},
    {104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127},
    {151, 150, 149, 148, 147, 146, 145, 144, 135, 134, 133, 132, 131, 130, 129, 128},
    {152, 153, 154, 155, 156, 157, 158, 159, 136, 137, 138, 139, 140, 141, 142, 143},
    {167, 166, 165, 164, 163, 162, 161, 160, 183, 182, 181, 180, 179, 178, 177, 176},
    {168, 169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190, 191},
    {215, 214, 213, 212, 211, 210, 209, 208, 199, 198, 197, 196, 195, 194, 193, 192},
    {216, 217, 218, 219, 220, 221, 222, 223, 200, 201, 202, 203, 204, 205, 206, 207},
    {231, 230, 229, 228, 227, 226, 225, 224, 247, 246, 245, 244, 243, 242, 241, 240},
    {232, 233, 234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254, 255}};

extern uint8_t gBright; // global brightness (0-255) (only scaling)

// forward declarations
void panel_setPixel(int8_t row, int8_t col, uint8_t brightness);

// leaves the panel on (but no PWM)
// call this once before doing a long blocking task, this keeps the panel on.
// after wards just call panel_refresh() to refresh the panel
void panel_hold();

void panel_timer_start();
void panel_timer_stop();

// refreshes the panel (with PWM)
// needed for animations and individual pixel brightness
void panel_refresh();

void panel_init();

void panel_fill(uint8_t col);

inline void panel_clear()
{
    panel_fill(0);
}

// draws a sprite starting at the top left corner (tlX, tlY)
// It is also possible to draw sprites that are larger than the panel or (partially) out of bounds, but they will be clipped.
void panel_drawSprite(int8_t tlX, int8_t tlY, const uint8_t *data, uint8_t width, uint8_t height);
