#include "config.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include <sys/stat.h>

static const char *NVS_NAMESPACE = "obegransad"; // NVS namespace
static const char *TAG = "NvsSettings";

/*
integer types: uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t,
int64_t zero-terminated string (4000 bytes max) variable length binary data
(blob, 508000 bytes max)
*/

//////////////////////
// NVS GETTER
//////////////////////

// Generic NVS getter for integer types (i8/u8/i16/u16/i32/u32/i64/u64)
template <typename T>
esp_err_t nvs_get_value(nvs_handle_t nvs_handle, const char *key, T &out,
                        T default_value);

// Macro to generate specializations for integer types
#define NVS_GET_VALUE_SPECIALIZATION(TYPE, NVS_FN)                             \
  template <>                                                                  \
  esp_err_t nvs_get_value<TYPE>(nvs_handle_t nvs_handle, const char *key,      \
                                TYPE &out, TYPE default_value) {               \
    esp_err_t err = NVS_FN(nvs_handle, key, &out);                             \
    if (err == ESP_ERR_NVS_NOT_FOUND) {                                        \
      out = default_value;                                                     \
    } else if (err != ESP_OK) {                                                \
      ESP_LOGE(TAG, "Error (%s) reading '%s' from NVS", esp_err_to_name(err),  \
               key);                                                           \
      nvs_close(nvs_handle);                                                   \
      return err;                                                              \
    }                                                                          \
    return ESP_OK;                                                             \
  }
// Generate specializations for integer types
NVS_GET_VALUE_SPECIALIZATION(uint8_t, nvs_get_u8)
NVS_GET_VALUE_SPECIALIZATION(int8_t, nvs_get_i8)
NVS_GET_VALUE_SPECIALIZATION(uint16_t, nvs_get_u16)
NVS_GET_VALUE_SPECIALIZATION(int16_t, nvs_get_i16)
NVS_GET_VALUE_SPECIALIZATION(uint32_t, nvs_get_u32)
NVS_GET_VALUE_SPECIALIZATION(int32_t, nvs_get_i32)
NVS_GET_VALUE_SPECIALIZATION(uint64_t, nvs_get_u64)
NVS_GET_VALUE_SPECIALIZATION(int64_t, nvs_get_i64)
#undef NVS_GET_VALUE_SPECIALIZATION

// manually implement string and blob getters
template <>
esp_err_t nvs_get_value<char *>(nvs_handle_t nvs_handle, const char *key,
                                char *&out, char *default_value) {
  size_t required_size;
  esp_err_t err = nvs_get_str(nvs_handle, key, nullptr, &required_size);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    out = default_value;
    return ESP_OK;
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) reading '%s' from NVS", esp_err_to_name(err),
             key);
    nvs_close(nvs_handle);
    return err;
  }

  err = nvs_get_str(nvs_handle, key, out, &required_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) reading '%s' from NVS", esp_err_to_name(err),
             key);
    nvs_close(nvs_handle);
    return err;
  }
  return ESP_OK;
}

//////////////////////
// NVS SETTER
//////////////////////

// Generic NVS setter for integer types (i8/u8/i16/u16/i32/u32/i64/u64)
template <typename T>
esp_err_t nvs_set_value(nvs_handle_t nvs_handle, const char *key, T value);

// Macro to generate specializations for integer types
#define NVS_SET_VALUE_SPECIALIZATION(TYPE, NVS_FN)                             \
  template <>                                                                  \
  esp_err_t nvs_set_value<TYPE>(nvs_handle_t nvs_handle, const char *key,      \
                                TYPE value) {                                  \
    esp_err_t err = NVS_FN(nvs_handle, key, value);                            \
    if (err != ESP_OK) {                                                       \
      ESP_LOGE(TAG, "Error (%s) writing '%s' to NVS", esp_err_to_name(err),    \
               key);                                                           \
      nvs_close(nvs_handle);                                                   \
      return err;                                                              \
    }                                                                          \
    return ESP_OK;                                                             \
  }

// Generate specializations for integer types
NVS_SET_VALUE_SPECIALIZATION(uint8_t, nvs_set_u8)
NVS_SET_VALUE_SPECIALIZATION(int8_t, nvs_set_i8)
NVS_SET_VALUE_SPECIALIZATION(uint16_t, nvs_set_u16)
NVS_SET_VALUE_SPECIALIZATION(int16_t, nvs_set_i16)
NVS_SET_VALUE_SPECIALIZATION(uint32_t, nvs_set_u32)
NVS_SET_VALUE_SPECIALIZATION(int32_t, nvs_set_i32)
NVS_SET_VALUE_SPECIALIZATION(uint64_t, nvs_set_u64)
NVS_SET_VALUE_SPECIALIZATION(int64_t, nvs_set_i64)
#undef NVS_SET_VALUE_SPECIALIZATION

// manually implement string and blob setters

template <>
esp_err_t nvs_set_value<char *>(nvs_handle_t nvs_handle, const char *key,
                                char *value) {
  esp_err_t err = nvs_set_str(nvs_handle, key, value);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) writing '%s' to NVS", esp_err_to_name(err), key);
    nvs_close(nvs_handle);
    return err;
  }
  return ESP_OK;
}

//////////////////////
// CONFIG
//////////////////////

esp_err_t nvs_read_settings(Settings &settings) {
  nvs_handle_t nvs_handle;

  // TODO: do i have to create the namespace first?

  ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle), TAG,
                      "Could not open NVS handle");

  Settings out = {};
  esp_err_t err;

  // Brightness day
  ESP_RETURN_ON_ERROR(nvs_get_value<uint8_t>(nvs_handle, "brightness_day",
                                             out.brightness_day, 200),
                      TAG, "Failed to read brightness_day");
  // Brightness night
  ESP_RETURN_ON_ERROR(nvs_get_value<uint8_t>(nvs_handle, "brightness_night",
                                             out.brightness_night, 20),
                      TAG, "Failed to read brightness_night");

  // off hours
  ESP_RETURN_ON_ERROR(
      nvs_get_value<uint32_t>(nvs_handle, "off_hours", out.off_hours, 0), TAG,
      "Failed to read off_hours");

  // weather latitude
  ESP_RETURN_ON_ERROR(nvs_get_value<double>(nvs_handle, "weather_latitude",
                                            out.weather_latitude, 0.0),
                      TAG, "Failed to read weather_latitude");

  nvs_close(nvs_handle);
  settings = out;
  return ESP_OK;
}

esp_err_t nvs_write_settings(const Settings &settings) {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE("NvsVariable", "Fehler (%s) beim Öffnen des NVS-Handles!",
             esp_err_to_name(err));
    return;
  }

  // `if constexpr` wird zur Kompilierzeit ausgewertet, um die korrekte
  // nvs_set_*-Funktion für den jeweiligen Datentyp T auszuwählen.
  if constexpr (std::is_same_v<T, bool>) {
    uint8_t val_to_save = _value;
    err = nvs_set_u8(nvs_handle, _key, val_to_save);
  } else if constexpr (std::is_same_v<T, int32_t>) {
    err = nvs_set_i32(nvs_handle, _key.c_st(), _value);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    err = nvs_set_u32(nvs_handle, _key, _value);
  } else if constexpr (std::is_same_v<T, std::string>) {
    err = nvs_set_str(nvs_handle, _key, _value);
  } else {
    // Für nicht unterstützte Typen kann hier ein Kompilierfehler erzeugt
    // werden.
    static_assert(sizeof(T) == 0,
                  "NVS-Speicherung für diesen Typ nicht implementiert!");
  }

  if (err != ESP_OK) {
    ESP_LOGE("NvsVariable", "Fehler (%s) beim Schreiben in den NVS!",
             esp_err_to_name(err));
  } else {
    ESP_LOGI("NvsVariable", "Wert für Schlüssel '%s' im NVS gespeichert.",
             _key);
  }

  // Änderungen in den Flash-Speicher schreiben.
  nvs_commit(nvs_handle);
  // Handle schließen.
  nvs_close(nvs_handle);
}

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
