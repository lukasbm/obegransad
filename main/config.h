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
  double weather_latitude;    // for weather scene (-90 to 90)
  double weather_longitude;   // for weather scene (-180 to 180)
  char timezone[64];          // Posix Timezone String, see timezones.csv
  uint8_t anniversary_day;    // for anniversary scene (1-31)
  uint8_t anniversary_month;  // for anniversary scene (1-12)
  uint8_t user_image[16][16]; // image for user image scene, stored in row-major
                              // order, 1 byte per pixel (grayscale, 0-255)
  bool game_of_life_start[16][16]; // initial state for Game of Life scene,
                                   // true = alive, false = dead
                                   // Index mapping: index = row * 16 + col,
                                   // where row and col are in [0, 15]
} __attribute__((packed));

/**
 * @brief Parse JSON string to Settings struct.
 * @param json Pointer to null-terminated JSON string.
 * @param settings Reference to Settings struct to fill.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t parse_settings_json(const char *, Settings &);

/**
 * @brief Serialize Settings struct to JSON string.
 * @param settings Reference to Settings struct to serialize.
 * @return Pointer to a newly allocated null-terminated JSON string.
 *         The caller is responsible for freeing the returned pointer using
 * free().
 */
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

/**
 * @brief Global configuration object.
 *
 * This global instance of Settings holds the current configuration for the
 * application. It should be initialized at startup, typically by reading from
 * NVS using nvs_read_settings(). Any changes to this object should be persisted
 * using nvs_write_settings().
 */
extern Settings g_settings;
