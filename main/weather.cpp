#include "weather.h"
#include "esp_tls.h"

#include <arpa/inet.h>
#include <cJSON.h>
#include <cstdio>
#include <ctime>
#include <esp_check.h>
#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_http_client.h>
#include <netdb.h>
#include <sys/socket.h>

#define MAX_HTTP_BUFFER 1024 * 5

static const char *TAG = "weather";

// Response buffer structure for chunked data handling
typedef struct {
  char *buffer;
  int capacity;
  int length;
} http_response_t;

esp_err_t event_handler(esp_http_client_event_t *evt) {
  http_response_t *response = (http_response_t *)evt->user_data;

  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
    break;

  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED - Connection established");
    // Reset response buffer for new request
    if (response && response->buffer) {
      response->length = 0;
      memset(response->buffer, 0, response->capacity);
    }
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

    if (!response || !response->buffer) {
      ESP_LOGE(TAG, "No response buffer provided");
      return ESP_FAIL;
    }

    // Check buffer capacity before copying
    if (response->length + evt->data_len >= response->capacity) {
      ESP_LOGE(TAG, "Response buffer overflow: %d + %d >= %d", response->length,
               evt->data_len, response->capacity);
      return ESP_FAIL;
    }

    // Copy chunk data to buffer
    memcpy(response->buffer + response->length, evt->data, evt->data_len);
    response->length += evt->data_len;

    ESP_LOGD(TAG, "Received chunk: %d bytes, total: %d/%d", evt->data_len,
             response->length, response->capacity);
    break;

  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    if (response && response->buffer && response->length > 0) {
      // Null terminate the response
      response->buffer[response->length] = '\0';
      ESP_LOGI(TAG, "HTTP response complete: %d bytes", response->length);
    }
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
    break;
  }

  case HTTP_EVENT_REDIRECT: {
    ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
    esp_http_client_set_redirection(evt->client);
    break;
  }

  default:
    // Other events (e.g. HTTP_EVENT_ON_STATUS_CODE / ON_HEADERS_COMPLETE added
    // in newer ESP-IDF) need no handling here.
    break;
  }
  return ESP_OK;
}

// the caller needs to free the body (when successful)
static esp_err_t request_weather_data(const char *url, char *response_data) {
  // Set up response structure for event handler
  http_response_t response = {
      .buffer = response_data, .capacity = MAX_HTTP_BUFFER, .length = 0};

  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.timeout_ms = 30000; // Increase timeout to 30 seconds for slow networks
  cfg.method = HTTP_METHOD_GET;
  cfg.transport_type = HTTP_TRANSPORT_OVER_SSL;
  cfg.crt_bundle_attach =
      esp_crt_bundle_attach;         // Use ESP-IDF certificate bundle
  cfg.use_global_ca_store = false;   // Use bundle instead of global store
  cfg.is_async = false;              // Synchronous requests
  cfg.event_handler = event_handler; // Event handler for response
  cfg.user_data = &response;         // Pass our response structure
  cfg.buffer_size = 1024;            // Reduce buffer size for embedded
  cfg.buffer_size_tx = 1024;         // Reduce TX buffer size

  // Initialize the HTTP client
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    return ESP_ERR_NO_MEM;
  }

  // Perform the HTTP request
  ESP_LOGD(TAG, "Starting HTTP request to OpenMeteo API...");
  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP request failed: %s (0x%x)", esp_err_to_name(err), err);
    esp_http_client_cleanup(client);
    return err;
  }

  // check status code
  int status_code = esp_http_client_get_status_code(client);
  ESP_LOGI(TAG, "HTTP response status: %d", status_code);
  if (status_code < 200 || status_code >= 300) {
    esp_http_client_cleanup(client);
    ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
    return ESP_ERR_INVALID_RESPONSE;
  }

  esp_http_client_cleanup(client);

  // Check if we received any data
  if (response.length <= 0) {
    ESP_LOGE(TAG, "No data received from server");
    return ESP_ERR_INVALID_RESPONSE;
  }

  ESP_LOGI(TAG, "Received %d bytes of weather data", response.length);

  return ESP_OK;
}

static esp_err_t parse_weather_data(char *json_body, WeatherData &out) {
  cJSON *root = cJSON_Parse(json_body);
  if (!root)
    return ESP_ERR_INVALID_RESPONSE;

  /* --- current weather --- */
  cJSON *current_weather = cJSON_GetObjectItem(root, "current");
  if (!current_weather) {
    cJSON_Delete(root);
    return ESP_ERR_INVALID_RESPONSE;
  }

  // TODO: need to validate 

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

  char *response_data = (char *)malloc(MAX_HTTP_BUFFER + 1);
  if (!response_data) {
    ESP_LOGE(TAG, "Failed to allocate memory for response data");
    return ESP_ERR_NO_MEM;
  }
  memset(response_data, 0, MAX_HTTP_BUFFER + 1); // Initialize buffer

  // 2. make request
  esp_err_t ret = request_weather_data(url, response_data);
  if (ret != ESP_OK) {
    free(response_data);
    ESP_LOGE(TAG, "Failed to fetch weather data: %s", esp_err_to_name(ret));
    return ret;
  }

  // 3. Parse JSON (will free the json body string)
  esp_err_t err = parse_weather_data(response_data, data);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to parse weather data: %s", esp_err_to_name(err));
  } else {
    weather_set(data); // publish to the cache scenes read from
  }

  free(response_data);
  return err;
}

// Cached snapshot of the most recently fetched weather. Read by scenes.
static WeatherData g_cached_weather;

WeatherData weather_get() { return g_cached_weather; }

void weather_set(const WeatherData &data) { g_cached_weather = data; }

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
