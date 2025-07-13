#include "weather.h"
#include "esp_tls.h"

#include <cJSON.h>
#include <cstdio>
#include <ctime>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_http_client.h>

#define MAX_HTTP_BUFFER 1024 * 5

// Forward declaration for certificate bundle
// FIXME: can i remove this?
extern "C" {
esp_err_t esp_crt_bundle_attach(void *conf);
}

static const char *TAG = "weather";

esp_err_t event_handler(esp_http_client_event_t *evt) {
  static char *output_buffer; // Buffer to store response of http request from
                              // event handler
  static int output_len;      // Stores number of bytes read
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
             evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    // Clean the buffer in case of a new request
    if (output_len == 0 && evt->user_data) {
      // we are just starting to copy the output data into the use
      memset(evt->user_data, 0, MAX_HTTP_BUFFER);
    }
    /*
     *  Check for chunked encoding is added as the URL for chunked encoding used
     * in this example returns binary data. However, event handler can also be
     * used in case chunked encoding is used.
     */
    if (!esp_http_client_is_chunked_response(evt->client)) {
      // If user_data buffer is configured, copy the response into the buffer
      int copy_len = 0;
      if (evt->user_data) {
        // The last byte in evt->user_data is kept for the NULL character in
        // case of out-of-bound access.
        copy_len = MIN(evt->data_len, (MAX_HTTP_BUFFER - output_len));
        if (copy_len) {
          memcpy(evt->user_data + output_len, evt->data, copy_len);
        }
      } else {
        int content_len = esp_http_client_get_content_length(evt->client);
        if (output_buffer == NULL) {
          // We initialize output_buffer with 0 because it is used by strlen()
          // and similar functions therefore should be null terminated.
          output_buffer = (char *)calloc(content_len + 1, sizeof(char));
          output_len = 0;
          if (output_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
            return ESP_FAIL;
          }
        }
        copy_len = MIN(evt->data_len, (content_len - output_len));
        if (copy_len) {
          memcpy(output_buffer + output_len, evt->data, copy_len);
        }
      }
      output_len += copy_len;
    }

    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    if (output_buffer != NULL) {
      free(output_buffer);
      output_buffer = NULL;
    }
    output_len = 0;
    break;
  case HTTP_EVENT_DISCONNECTED: {
    ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
    int mbedtls_err = 0;
    esp_err_t err = esp_tls_get_and_clear_last_error(
        (esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
    if (err != 0) {
      ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
      ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
    }
    if (output_buffer != NULL) {
      free(output_buffer);
      output_buffer = NULL;
    }
    output_len = 0;
    break;
  }

  case HTTP_EVENT_REDIRECT: {
    ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
    esp_http_client_set_header(evt->client, "From", "user@example.com");
    esp_http_client_set_header(evt->client, "Accept", "text/html");
    esp_http_client_set_redirection(evt->client);
    break;
  }
  }
  return ESP_OK;
}

// the caller needs to free the body (when successful)
static esp_err_t request_weather_data(const char *url, char *&out_body) {
  char *response_data = (char *)malloc(MAX_HTTP_BUFFER + 1);
  if (!response_data) {
    ESP_LOGE(TAG, "Failed to allocate memory for response data");
    return ESP_ERR_NO_MEM;
  }
  memset(response_data, 0, MAX_HTTP_BUFFER + 1); // Initialize buffer

  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.timeout_ms = 8000;
  cfg.method = HTTP_METHOD_GET;
  cfg.transport_type = HTTP_TRANSPORT_OVER_SSL;
  cfg.crt_bundle_attach =
      esp_crt_bundle_attach;         // Use ESP-IDF certificate bundle
  cfg.use_global_ca_store = false;   // Use bundle instead of global store
  cfg.is_async = false;              // Synchronous requests
  cfg.event_handler = event_handler; // Event handler for response
  cfg.user_data = &response_data;    // Pass our response buffer

  // Initialize the HTTP client
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    return ESP_ERR_NO_MEM;
  }

  // Perform the HTTP request
  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return err;
  }

  // check status code
  int status_code = esp_http_client_get_status_code(client);
  if (status_code < 200 || status_code >= 300) {
    esp_http_client_cleanup(client);
    ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
    return ESP_ERR_INVALID_RESPONSE;
  }

  esp_http_client_cleanup(client);

  out_body = response_data; // Pass the response data to the caller
  return ESP_OK;
}

static esp_err_t parse_weather_data(char *json_body, WeatherData &out) {
  cJSON *root = cJSON_Parse(json_body);
  free(json_body);
  if (!root)
    return ESP_ERR_INVALID_RESPONSE;

  /* --- current weather --- */
  cJSON *current_weather = cJSON_GetObjectItem(root, "current");
  if (!current_weather) {
    cJSON_Delete(root);
    return ESP_ERR_INVALID_RESPONSE;
  }

  // FIXME: is this parsing for timestamps correct?
  out.requestTime = static_cast<time_t>(
      cJSON_GetObjectItem(current_weather, "time")->valueint);
  out.temperature = static_cast<float>(
      cJSON_GetObjectItem(current_weather, "temperature_2m")->valuedouble);
  out.weatherCode = static_cast<WeatherCode>(
      cJSON_GetObjectItem(current_weather, "weather_code")->valueint);
  out.isDay = cJSON_GetObjectItem(current_weather, "is_day")->valueint == 1
                  ? true
                  : false;

  /* --- daily arrays --- */
  cJSON *daily_weather = cJSON_GetObjectItem(root, "daily");
  if (!daily_weather) {
    cJSON_Delete(root);
    return ESP_ERR_INVALID_RESPONSE;
  }

  cJSON *dates = cJSON_GetObjectItem(daily_weather, "time");
  cJSON *wx_codes = cJSON_GetObjectItem(daily_weather, "weather_code");
  cJSON *t_max = cJSON_GetObjectItem(daily_weather, "temperature_2m_max");
  cJSON *t_min = cJSON_GetObjectItem(daily_weather, "temperature_2m_min");
  cJSON *t_mean = cJSON_GetObjectItem(daily_weather, "temperature_2m_mean");
  cJSON *uvi_max = cJSON_GetObjectItem(daily_weather, "uv_index_max");
  cJSON *sunrise = cJSON_GetObjectItem(daily_weather, "sunrise");
  cJSON *sunset = cJSON_GetObjectItem(daily_weather, "sunset");

  for (int i = 0; i < FORECAST_DAYS && i < cJSON_GetArraySize(dates); ++i) {
    out.daily[i].sunrise =
        static_cast<time_t>(cJSON_GetArrayItem(sunrise, i)->valueint);
    out.daily[i].sunset =
        static_cast<time_t>(cJSON_GetArrayItem(sunset, i)->valueint);
    out.daily[i].uvIndexMax =
        static_cast<float>(cJSON_GetArrayItem(uvi_max, i)->valuedouble);
    out.daily[i].temperatureMax =
        static_cast<float>(cJSON_GetArrayItem(t_max, i)->valuedouble);
    out.daily[i].temperatureMin =
        static_cast<float>(cJSON_GetArrayItem(t_min, i)->valuedouble);
    out.daily[i].temperatureMean =
        static_cast<float>(cJSON_GetArrayItem(t_mean, i)->valuedouble);
    out.daily[i].weatherCode =
        static_cast<WeatherCode>(cJSON_GetArrayItem(wx_codes, i)->valueint);
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

  ESP_LOGI(TAG, "Fetching weather data from URL: %s", url);

  // 2. Make request
  char *json_body = nullptr;
  ESP_RETURN_ON_ERROR(request_weather_data(url, json_body), TAG,
                      "Failed to fetch weather data");

  // 3. Parse JSON (will free the json body string)
  ESP_RETURN_ON_ERROR(parse_weather_data(json_body, data), TAG,
                      "Failed to parse weather data");

  return ESP_OK;
}

void WeatherData::print() const {
  ESP_LOGI(TAG,
           "Weather Data: RequestTime: %u, temperature: %.2f, weatherCode: "
           "%d, isDay: %d",
           (unsigned int)requestTime, temperature, weatherCode, isDay);
  for (int i = 0; i < FORECAST_DAYS; ++i) {
    ESP_LOGI(
        TAG,
        "Day %d: sunrise: %u, sunset: %u, uvIndexMax: %.2f, temperatureMax: "
        "%.2f, temperatureMin: %.2f, temperatureMean: %.2f, weatherCode: %d",
        i + 1, (unsigned int)daily[i].sunrise, (unsigned int)daily[i].sunset,
        daily[i].uvIndexMax, daily[i].temperatureMax, daily[i].temperatureMin,
        daily[i].temperatureMean, daily[i].weatherCode);
  }
}
