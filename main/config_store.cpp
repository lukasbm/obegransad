#include "config_store.h"

#include "clock.h"
#include "ikea-obegransad-panel.h"
#include "sdkconfig.h"
#include "weather_client.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>

static const char *TAG = "config";

static constexpr const char *NVS_NAMESPACE = "obgcfg";

// Loaded values; defaults come from Kconfig.
static char s_timezone[64] = CONFIG_OBG_TIMEZONE;
static double s_latitude = 0.0;
static double s_longitude = 0.0;
static int s_anniversary_day = CONFIG_OBG_ANNIVERSARY_DAY;
static int s_anniversary_month = CONFIG_OBG_ANNIVERSARY_MONTH;
static uint8_t s_brightness = 255;
static bool s_initialized = false;

static esp_err_t load_string(nvs_handle_t handle, const char *key, char *out,
                             size_t out_size) {
  size_t len = out_size;
  return nvs_get_str(handle, key, out, &len);
}

esp_err_t config_store_init(void) {
  s_latitude = atof(CONFIG_OBG_WEATHER_LATITUDE);
  s_longitude = atof(CONFIG_OBG_WEATHER_LONGITUDE);

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "NVS open failed (%s); using defaults", esp_err_to_name(err));
    s_initialized = true;
    return err;
  }

  if (load_string(handle, "tz", s_timezone, sizeof(s_timezone)) != ESP_OK) {
    strlcpy(s_timezone, CONFIG_OBG_TIMEZONE, sizeof(s_timezone));
  }
  char buf[32];
  if (load_string(handle, "lat", buf, sizeof(buf)) == ESP_OK) {
    s_latitude = atof(buf);
  }
  if (load_string(handle, "lon", buf, sizeof(buf)) == ESP_OK) {
    s_longitude = atof(buf);
  }
  uint8_t u8 = 0;
  if (nvs_get_u8(handle, "ann_d", &u8) == ESP_OK && u8 >= 1 && u8 <= 31) {
    s_anniversary_day = u8;
  }
  if (nvs_get_u8(handle, "ann_m", &u8) == ESP_OK && u8 >= 1 && u8 <= 12) {
    s_anniversary_month = u8;
  }
  if (nvs_get_u8(handle, "bright", &u8) == ESP_OK) {
    s_brightness = u8;
  }
  nvs_close(handle);

  s_initialized = true;
  ESP_LOGI(TAG, "config: tz='%s' location=%.4f,%.4f anniversary=%d.%d "
                "brightness=%u",
           s_timezone, s_latitude, s_longitude, s_anniversary_day,
           s_anniversary_month, s_brightness);
  return ESP_OK;
}

void config_store_apply(void) {
  if (!s_initialized) {
    return;
  }
  clock_apply_timezone(s_timezone);
  panel_set_global_brightness(s_brightness);
}

static esp_err_t store_u8(nvs_handle_t handle, const char *key, uint8_t value) {
  return nvs_set_u8(handle, key, value);
}

static esp_err_t commit(nvs_handle_t handle) {
  esp_err_t err = nvs_commit(handle);
  nvs_close(handle);
  return err;
}

esp_err_t config_set_timezone(const char *tz) {
  if (tz == nullptr || tz[0] == '\0' || strlen(tz) >= sizeof(s_timezone)) {
    return ESP_ERR_INVALID_ARG;
  }
  strlcpy(s_timezone, tz, sizeof(s_timezone));
  clock_apply_timezone(s_timezone);

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }
  err = nvs_set_str(handle, "tz", s_timezone);
  if (err == ESP_OK) {
    err = commit(handle);
  } else {
    nvs_close(handle);
  }
  return err;
}

esp_err_t config_set_location(double latitude, double longitude) {
  if (std::isnan(latitude) || std::isnan(longitude) || latitude < -90.0 ||
      latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
    return ESP_ERR_INVALID_ARG;
  }
  s_latitude = latitude;
  s_longitude = longitude;

  char buf[32];
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }
  snprintf(buf, sizeof(buf), "%.6f", s_latitude);
  err = nvs_set_str(handle, "lat", buf);
  if (err == ESP_OK) {
    snprintf(buf, sizeof(buf), "%.6f", s_longitude);
    err = nvs_set_str(handle, "lon", buf);
  }
  if (err == ESP_OK) {
    err = commit(handle);
  } else {
    nvs_close(handle);
  }

  // Fresh coordinates: fetch new weather right away.
  weather_client_request_fetch();
  return err;
}

esp_err_t config_set_anniversary(int day, int month) {
  if (day < 1 || day > 31 || month < 1 || month > 12) {
    return ESP_ERR_INVALID_ARG;
  }
  s_anniversary_day = day;
  s_anniversary_month = month;

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }
  err = store_u8(handle, "ann_d", (uint8_t)day);
  if (err == ESP_OK) {
    err = store_u8(handle, "ann_m", (uint8_t)month);
  }
  if (err == ESP_OK) {
    err = commit(handle);
  } else {
    nvs_close(handle);
  }
  return err;
}

esp_err_t config_set_brightness(uint8_t brightness) {
  s_brightness = brightness;
  panel_set_global_brightness(brightness);

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }
  err = store_u8(handle, "bright", brightness);
  if (err == ESP_OK) {
    err = commit(handle);
  } else {
    nvs_close(handle);
  }
  return err;
}

const char *config_get_timezone(void) { return s_timezone; }
double config_get_latitude(void) { return s_latitude; }
double config_get_longitude(void) { return s_longitude; }
int config_get_anniversary_day(void) { return s_anniversary_day; }
int config_get_anniversary_month(void) { return s_anniversary_month; }
uint8_t config_get_brightness(void) { return s_brightness; }
