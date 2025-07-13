#include "esp_err.h"
#include <cstdint>
#include <string>

// trying to use the names consistent between this struct and json
// TODO: in the future consider a modular approach where each scene or other plugin can add its own settings.
struct Settings {
  uint8_t brightness_day, brightness_night;
  uint32_t off_hours; // bit mask of 24 bits, each bit represents an hour of the
                      // day (0-23), LSB is 0:00, "MSB" is 23:00
  double weather_latitude, weather_longitude;
  char *timezone;
  uint8_t anniversary_day, anniversary_month;
};

esp_err_t parse_settings(const std::string &json, Settings &settings);
esp_err_t output_settings(const std::string &json, const Settings &settings);
