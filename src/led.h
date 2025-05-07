#pragma once

#include <Arduino.h>

// I dont know which microcontroller the OBEGRÄNSAD uses,
// but it might be one of the classics:
// MBI5024/5034, STP16, TPIC6B595

/*
DI	Serial data in	1 bit at a time, no concept of “brightness”
CLK	Shift‑register clock	Just marches data along
CLA	Latch / strobe	Copies the 256 bits into the output on/off latches
EN	Output‑enable / blank	Global: either all LED outputs drive, or none do
*/
#define P_EN D1
#define P_DI D2
#define P_CLK D3
#define P_CLA D4

/*
The panel works as follows:
1. Shift the data for one bit-plane
2. Latch it with CLA.
3. Turn on EN for a slice of time. EN therefore acts as a PWM signal that drives the brightness.
4. Blank (EN = low) before the next bit-plane is shifted in.
The entire display can be seen as a large shift register.
*/

// timing: BASE_US × (2^8 − 1) == full frame duration
// 40 µs gives ≈ 78 Hz refresh; 20 µs → 156 Hz, etc.
#define BASE_TIME 160 // 19.5 Hz

// LUT For OBEGRÄNSAD (they are wired weirdly)
static int lut[16][16] = {
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

static uint8_t panel_buf[16 * 16]; // grayscale graphics buffer

// forward declarations
void panel_setPixel(int8_t row, int8_t col, uint8_t brightness);
void panel_show();
void panel_printChar(uint8_t xs, uint8_t ys, char ch);
void panel_init();
void panel_fill(uint8_t col);
inline void panel_clear()
{
    panel_fill(0);
}
