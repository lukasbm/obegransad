#pragma once

#include "esp_err.h"

#include <cstdint>

// trying to use the names consistent between this struct and json

struct Settings {
  uint8_t brightness_day;
  uint8_t brightness_night;
  uint32_t off_hours; // bit mask of 24 bits, each bit represents an hour of the
                      // day (0-23), LSB is 0:00, "MSB" is 23:00
  double weather_latitude;
  double weather_longitude;
  char timezone[64];
  uint8_t anniversary_day;
  uint8_t anniversary_month;
} __attribute__((packed));

esp_err_t parse_settings_json(const char *, Settings &);
esp_err_t serialize_settings_json(char *, const Settings &);

esp_err_t nvs_read_settings(Settings &settings);
esp_err_t nvs_write_settings(const Settings &settings);

extern Settings g_settings; // global config object

// TODO: in the future consider a modular approach where each scene or other
// plugin can add its own settings.
// Possible object oriented structure for config:
// It would allow for the plugin system to add its own config items and easily
// add other backends for storage.
//    Config
//    ConfigGroup
//    ConfigItem
//    ConfigItemString : ConfigItem
//    ConfigItemNumeric : ConfigItem
//    ConfigItemBlob : ConfigItem
//    ConfigArray : ConfigGroup
//    ConfigObject : ConfigGroup
