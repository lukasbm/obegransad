
/**
 * @brief This header file describes a custom image file format for ESP
 * embedding.
 * It is specifically a black and white image format with variable color depth.
 * It does not use any compression or pallete, hence it can be streamed after
 * reading the header.
 * @note imgbwb stands for "image black and white bitmap"
 */

#include <cstdint>
struct imgbwb_header_t {
  uint8_t w; // Image/Sprite width in pixels
  uint8_t h; // Image/Sprite height in pixels
} __attribute__((packed));

inline imgbwb_header_t imgb_get_header(const uint8_t *data) {
  if (!data) {
    return {0, 0}; // Return an empty header if data is null
  }
  imgbwb_header_t header;
  header.w = data[0];
  header.h = data[1];
  return header;
}
