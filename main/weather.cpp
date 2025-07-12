#include "weather.h"

#include "esp_check.h"
#include "esp_err.h"
#include <cstdio>
#include <ctime>
#include <esp_err.h>
#include <string.h>

// the caller needs to free the body (when successful)
static esp_err_t request_weather_data(const char *url, char *&out_body) {
  esp_http_client_config_t cfg = {0};
  cfg.url = url;
  cfg.timeout_ms = 8000;
  cfg.method = HTTP_METHOD_GET;

  esp_http_client_handle_t cli = esp_http_client_init(&cfg);
  if (!cli)
    return ESP_ERR_NO_MEM;

  esp_err_t err = esp_http_client_perform(cli);
  if (err != ESP_OK) {
    esp_http_client_cleanup(cli);
    return err;
  }

  int len = esp_http_client_get_content_length(cli);
  if (len <= 0) {
    esp_http_client_cleanup(cli);
    return ESP_ERR_INVALID_SIZE;
  }

  int status_code = esp_http_client_get_status_code(cli);
  if (status_code < 200 || status_code >= 300) {
    esp_http_client_cleanup(cli);
    ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
    return ESP_ERR_INVALID_RESPONSE;
  }

  // Allocate memory for the response body
  if (len > 1024 * 10) { // limit to 10KB
    esp_http_client_cleanup(cli);
    ESP_LOGE(TAG, "Response body too large: %d bytes", len);
    return ESP_ERR_INVALID_SIZE;
  }

  char *json_body = (char *)malloc(len + 1);
  if (!json_body) {
    esp_http_client_cleanup(cli);
    return ESP_ERR_NO_MEM;
  }

  int r = esp_http_client_read(cli, json_body, len);
  esp_http_client_cleanup(cli); // Cleanup the client after reading
  if (r != len) {
    free(json_body);
    return ESP_FAIL;
  }
  json_body[len] = '\0';

  out_body = json_body; // Set the output body to the caller
  return ESP_OK;
}

static esp_err_t parse_weather_data(char *json_body, WeatherData &out) {
  cJSON *root = cJSON_Parse(json_body);
  free(json_body);
  if (!root)
    return ESP_ERR_INVALID_RESPONSE;

  /* --- current weather --- */
  cJSON *current_weather = cJSON_GetObjectItem(root, "current_weather");
  if (!current_weather) {
    cJSON_Delete(root);
    return ESP_ERR_INVALID_RESPONSE;
  }

  out.requestTime = time(NULL); // FIXME: local fetch time
  out.temperature =
      (float)cJSON_GetObjectItem(current_weather, "temperature")->valuedouble;
  out.weatherCode =
      (WeatherCode)cJSON_GetObjectItem(current_weather, "weathercode")
          ->valueint;
  out.isDay =
      cJSON_GetObjectItem(current_weather, "is_day")->valueint ? true : false;

  /* --- daily arrays --- */
  cJSON *daily_weather = cJSON_GetObjectItem(root, "daily");

  cJSON *dates = cJSON_GetObjectItem(daily_weather, "time");
  cJSON *wx_codes = cJSON_GetObjectItem(daily_weather, "weather_code");
  cJSON *t_max = cJSON_GetObjectItem(daily_weather, "temperature_2m_max");
  cJSON *t_min = cJSON_GetObjectItem(daily_weather, "temperature_2m_min");
  cJSON *t_mean = cJSON_GetObjectItem(daily_weather, "temperature_2m_mean");
  cJSON *uvi_max = cJSON_GetObjectItem(daily_weather, "uv_index_max");
  cJSON *sunrise = cJSON_GetObjectItem(daily_weather, "sunrise");
  cJSON *sunset = cJSON_GetObjectItem(daily_weather, "sunset");

  for (int i = 0; i < FORECAST_DAYS && i < cJSON_GetArraySize(dates); ++i) {
    out.daily[i].sunrise = time_t(cJSON_GetArrayItem(sunrise, i)->valuestring);
    out.daily[i].sunset = time_t(cJSON_GetArrayItem(sunset, i)->valuestring);
    out.daily[i].uvIndexMax =
        (float)cJSON_GetArrayItem(uvi_max, i)->valuedouble;
    out.daily[i].temperatureMax =
        (float)cJSON_GetArrayItem(t_max, i)->valuedouble;
    out.daily[i].temperatureMin =
        (float)cJSON_GetArrayItem(t_min, i)->valuedouble;
    out.daily[i].temperatureMean =
        (float)cJSON_GetArrayItem(t_mean, i)->valuedouble;
    out.daily[i].weatherCode =
        (WeatherCode)cJSON_GetArrayItem(wx_codes, i)->valueint;
  }

  cJSON_Delete(root);
  return ESP_OK;
}

esp_err_t fetch_weather(float latitude, float longitude, WeatherData &data) {
  // 1. Prepare URL
  // Example URL:
  // https://api.open-meteo.com/v1/forecast?latitude=49.4542&longitude=11.0775&daily=sunrise,sunset,uv_index_max,temperature_2m_max,temperature_2m_min,weather_code,temperature_2m_mean&current=temperature_2m,weather_code,is_day&timezone=auto&timeformat=unixtime
  char url[512];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast"
           "?latitude=%.6f&longitude=%.6f"
           "&daily=sunrise,sunset,uv_index_max,temperature_2m_max,temperature_"
           "2m_min,weather_code,temperature_2m_mean"
           "&current=temperature_2m,weather_code,is_day"
           "&timezone=auto"
           "&timeformat=unixtime"
           "&forecast_days=%d",
           latitude, longitude, FORECAST_DAYS);

  // 2. Make request
  char *json_body = nullptr;
  ESP_RETURN_ON_ERROR(request_weather_data(url, json_body), TAG,
                      "Failed to fetch weather data");

  // 3. Parse JSON
  ESP_RETURN_ON_ERROR(parse_weather_data(json_body, data), TAG,
                      "Failed to parse weather data");

  return ESP_OK;
}
