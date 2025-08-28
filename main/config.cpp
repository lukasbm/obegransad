#include "config.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include <array>
#include <cJSON.h>
#include <cstdint>
#include <cstring>
#include <sys/stat.h>

static const char *NVS_NAMESPACE = "obegransad"; // NVS namespace
static const char *TAG = "settings";

///////////////////////////////////
// Global object and defaults //
///////////////////////////////////

static Settings create_default_settings() {
  Settings settings = {};

  // scalar defaults
  settings.brightness_day = 200;
  settings.brightness_night = 20;
  settings.off_hours = 0;
  settings.weather_latitude = 0.0;
  settings.weather_longitude = 0.0;
  std::strncpy(settings.timezone, "UTC", sizeof(settings.timezone));
  settings.anniversary_day = 1;
  settings.anniversary_month = 1;

  // Initialize game_of_life_start with default pattern (Endless glider pattern)
  settings.game_of_life_start[1][2] = true;
  settings.game_of_life_start[2][3] = true;
  settings.game_of_life_start[3][1] = true;
  settings.game_of_life_start[3][2] = true;
  settings.game_of_life_start[3][3] = true;

  // default user image (simple smiley face pattern)
  settings.user_image[4][4] = 255;
  settings.user_image[4][11] = 255;
  settings.user_image[10][5] = 255;
  settings.user_image[10][6] = 255;
  settings.user_image[10][9] = 255;
  settings.user_image[10][10] = 255;
  for (int i = 5; i <= 10; ++i) {
    settings.user_image[12][i] = 255;
  }

  return settings;
}

Settings g_settings; // global settings object, initialized with default values

///////////////////////////////////
// NVS //
///////////////////////////////////

/*
integer types: uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t,
int64_t zero-terminated string (4000 bytes max) variable length binary data
(blob, 508000 bytes max)

Some useful links:
-
https://www.reddit.com/r/esp32/comments/vjv87u/can_i_prevent_espidf_from_updating_my_nvs_version/
- https://esp32.com/viewtopic.php?t=1097
*/

esp_err_t nvs_read_settings(Settings &config) {
  nvs_handle_t nvs_handle;

  // namespace is create if it does not exist (hence R/W mode!)
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Could not open NVS handle for reading: %s",
             esp_err_to_name(err));
    return err;
  }

  Settings out = create_default_settings();
  size_t required_size = sizeof(out);
  err = nvs_get_blob(nvs_handle, "settings", &out, &required_size);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "Settings not found, using default values");
    // If settings are not found, we can initialize with default values
    err = ESP_OK;
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read settings from NVS: %s", esp_err_to_name(err));
  }
  nvs_close(nvs_handle);
  config = out;
  return err;
}

esp_err_t nvs_write_settings(const Settings &config) {
  nvs_handle_t nvs_handle;
  ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle), TAG,
                      "Could not open NVS handle for writing");

  esp_err_t err = nvs_set_blob(nvs_handle, "settings", &config, sizeof(config));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write settings to NVS: %s", esp_err_to_name(err));
    nvs_close(nvs_handle);
    return err;
  }

  nvs_commit(nvs_handle);
  nvs_close(nvs_handle);
  return ESP_OK;
}

///////////////////////////////////
// Parsing and Serializing //
///////////////////////////////////

// Serializes the settings struct into a JSON string using cJSON.
// The caller must provide a pre-allocated output buffer with sufficient size.
// The function copies the JSON string into the output buffer and ensures null
// termination. Returns ESP_OK on success, or an error code on failure.
char *serialize_settings_json(const Settings &settings) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    ESP_LOGE(TAG, "Failed to create JSON object");
    return nullptr;
  }
  // brightness day
  if (!cJSON_AddNumberToObject(root, "brightness_day",
                               settings.brightness_day)) {
    ESP_LOGE(TAG, "Failed to add 'brightness_day' to JSON");
    cJSON_Delete(root);
    return nullptr;
  }
  // brightness night
  if (!cJSON_AddNumberToObject(root, "brightness_night",
                               settings.brightness_night)) {
    ESP_LOGE(TAG, "Failed to add 'brightness_night' to JSON");
    cJSON_Delete(root);
    return nullptr;
  }
  // off hours
  cJSON *off_hours_array = cJSON_CreateArray();
  if (!off_hours_array) {
    ESP_LOGE(TAG, "Failed to create off_hours array");
    cJSON_Delete(root);
    return nullptr;
  }
  for (int i = 0; i < 24; ++i) {
    if (!cJSON_AddItemToArray(
            off_hours_array, cJSON_CreateBool((settings.off_hours >> i) & 1))) {
      ESP_LOGE(TAG, "Failed to add item to 'off_hours' array");
      cJSON_Delete(off_hours_array);
      cJSON_Delete(root);
      return nullptr;
    }
  }
  if (!cJSON_AddItemToObject(root, "off_hours", off_hours_array)) {
    ESP_LOGE(TAG, "Failed to add 'off_hours' array to JSON");
    cJSON_Delete(off_hours_array);
    cJSON_Delete(root);
    return nullptr;
  }
  // weather latitude
  if (!cJSON_AddNumberToObject(root, "weather_latitude",
                               settings.weather_latitude)) {
    ESP_LOGE(TAG, "Failed to add 'weather_latitude' to JSON");
    cJSON_Delete(root);
    return nullptr;
  }
  // weather longitude
  if (!cJSON_AddNumberToObject(root, "weather_longitude",
                               settings.weather_longitude)) {
    ESP_LOGE(TAG, "Failed to add 'weather_longitude' to JSON");
    cJSON_Delete(root);
    return nullptr;
  }
  // timezone
  if (!cJSON_AddStringToObject(root, "timezone", settings.timezone)) {
    ESP_LOGE(TAG, "Failed to add 'timezone' to JSON");
    cJSON_Delete(root);
    return nullptr;
  }
  // anniversary day
  cJSON_AddNumberToObject(root, "anniversary_day", settings.anniversary_day);
  // anniversary month
  cJSON_AddNumberToObject(root, "anniversary_month",
                          settings.anniversary_month);

  // Print the JSON object to a string
  char *json_string = cJSON_Print(root);
  if (!json_string) {
    ESP_LOGE(TAG, "Failed to print JSON to string");
    cJSON_Delete(root);
    return nullptr;
  }

  // Copy the JSON string to the output buffer, including null terminator
  size_t json_len = strlen(json_string);
  char *output = (char *)malloc(json_len + 2);
  if (!output) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON output");
    cJSON_free(json_string);
    cJSON_Delete(root);
    return nullptr;
  }
  strncpy(output, json_string, json_len + 1); // include null terminator

  cJSON_free(json_string);
  cJSON_Delete(root);

  return output;
}

// parse setting from json string into settings struct using cJson
esp_err_t parse_settings_json(const char *json, Settings &settings) {
  cJSON *root = cJSON_Parse(json);
  if (!root) {
    ESP_LOGE(TAG, "Failed to parse JSON");
    return ESP_ERR_INVALID_ARG;
  }

  cJSON *item;
  Settings out = {};

  // Parse brightness_day
  item = cJSON_GetObjectItemCaseSensitive(root, "brightness_day");
  if (!item || !cJSON_IsNumber(item)) {
    ESP_LOGE(TAG, "Invalid or missing 'brightness_day' in JSON");
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  } else {
    out.brightness_day = (uint8_t)item->valueint;
  }

  // Parse brightness_night
  item = cJSON_GetObjectItemCaseSensitive(root, "brightness_night");
  if (!item || !cJSON_IsNumber(item)) {
    ESP_LOGE(TAG, "Invalid or missing 'brightness_night' in JSON");
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  } else {
    out.brightness_night = (uint8_t)item->valueint;
  }

  // Parse off_hours array
  item = cJSON_GetObjectItemCaseSensitive(root, "off_hours");
  if (!item || !cJSON_IsArray(item)) {
    ESP_LOGE(TAG, "Invalid or missing 'off_hours' in JSON");
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  } else {
    uint32_t off_hours_mask = 0;
    for (int i = 0; i < cJSON_GetArraySize(item); i++) {
      cJSON *hour_item = cJSON_GetArrayItem(item, i);
      if (cJSON_IsBool(hour_item) && cJSON_IsTrue(hour_item)) {
        off_hours_mask |= (1 << i);
      } else {
        ESP_LOGE(TAG, "Invalid value in 'off_hours' array at index %d", i);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
      }
    }
    out.off_hours = off_hours_mask;
  }

  // Parse weather latitude
  item = cJSON_GetObjectItemCaseSensitive(root, "weather_latitude");
  if (!item || !cJSON_IsNumber(item)) {
    ESP_LOGE(TAG, "Invalid or missing 'weather_latitude' in JSON");
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  } else {
    out.weather_latitude = item->valuedouble;
  }

  // Parse weather longitude
  item = cJSON_GetObjectItemCaseSensitive(root, "weather_longitude");
  if (!item || !cJSON_IsNumber(item)) {
    ESP_LOGE(TAG, "Invalid or missing 'weather_longitude' in JSON");
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  } else {
    out.weather_longitude = item->valuedouble;
  }

  // Parse timezone
  item = cJSON_GetObjectItemCaseSensitive(root, "timezone");
  if (!item || !cJSON_IsString(item) || (item->valuestring == NULL)) {
    ESP_LOGE(TAG, "Invalid or missing 'timezone' in JSON");
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  } else {
    // Ensure the timezone string is null-terminated
    out.timezone[0] = '\0'; // Initialize to empty string
    if (strlen(item->valuestring) < sizeof(out.timezone)) {
      strncpy(out.timezone, item->valuestring, sizeof(out.timezone) - 1);
      out.timezone[sizeof(out.timezone) - 1] = '\0'; // Ensure null termination
    } else {
      ESP_LOGE(TAG, "Timezone string too long");
      cJSON_Delete(root);
      return ESP_ERR_INVALID_ARG;
    }
  }

  // Parse anniversary_day
  item = cJSON_GetObjectItemCaseSensitive(root, "anniversary_day");
  if (!item || !cJSON_IsNumber(item)) {
    ESP_LOGE(TAG, "Invalid or missing 'anniversary_day' in JSON");
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  } else {
    out.anniversary_day = (uint8_t)item->valueint;
  }

  // Parse anniversary_month
  item = cJSON_GetObjectItemCaseSensitive(root, "anniversary_month");
  if (!item || !cJSON_IsNumber(item)) {
  } else {
    out.anniversary_month = (uint8_t)item->valueint;
  }

  // successfully parsed all items
  cJSON_Delete(root);
  // only replace if entire json was parsed successfully
  settings = out;
  return ESP_OK;
}
