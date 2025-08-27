#pragma once

#include "esp_err.h"

#include <cstdint>

// trying to use the names consistent between this struct and json

/**
 * @brief Settings struct, packed to avoid padding bytes
 */
struct Settings {
  uint8_t brightness_day;   // global brightness settings
  uint8_t brightness_night; // global brightness settings
  uint32_t off_hours; // bit mask of 24 bits, each bit represents an hour of the
                      // day (0-23), LSB is 0:00, "MSB" is 23:00
  double weather_latitude;   // for weather scene (-90 to 90)
  double weather_longitude;  // for weather scene (-180 to 180)
  char timezone[64];         // Posix Timezone String, see timezones.csv
  uint8_t anniversary_day;   // for anniversary scene (1-31)
  uint8_t anniversary_month; // for anniversary scene (1-12)
  uint8_t user_image[256]; // 16x16 image for user image scene, 1 byte per pixel
} __attribute__((packed));

// TODO: allow for partial updates of the settings
esp_err_t parse_settings_json(const char *, Settings &);
char *serialize_settings_json(const Settings &);

/**
 * @brief Read settings from NVS
 * @param settings Reference to Settings struct to fill
 * @return ESP_OK on success, error code otherwise
 * @note always reads the full struct in a single operation/transaction
 */
esp_err_t nvs_read_settings(Settings &settings);

/**
 * @brief Write settings to NVS
 * @param settings Reference to Settings struct to write
 * @return ESP_OK on success, error code otherwise
 * @note always writes the full struct in a single operation/transaction
 */
esp_err_t nvs_write_settings(const Settings &settings);

extern Settings g_settings; // global config object
