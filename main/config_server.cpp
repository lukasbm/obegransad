// HTTP config server. See docs/openapi.yaml for the API contract.
//
// Handlers run on the esp_http_server task(s): they only read state, and
// control actions are queued via app_control_request() for the main task. Keep
// handlers short.

#include "config_server.h"

#include "app_control.h"
#include "app_events.h"
#include "clock.h"
#include "config_page.h"
#include "config_store.h"
#include "device.h" // wifi_check()
#include "helper.hpp" // millis()
#include "ikea-obegransad-panel.h"
#include "presets.hpp"
#include "scene_switcher.h"
#include "sdkconfig.h"
#include "state.h"
#include "weather.h"

#include <cJSON.h>
#include <cstdlib>
#include <cstring>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>

static const char *TAG = "config_server";

// --- helpers -----------------------------------------------------------------

static esp_err_t send_json(httpd_req_t *req, cJSON *root) {
  char *body = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (body == nullptr) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "JSON encoding failed");
  }
  httpd_resp_set_type(req, "application/json");
  const esp_err_t err = httpd_resp_sendstr(req, body);
  free(body);
  return err;
}

static esp_err_t send_error(httpd_req_t *req, const char *status,
                            const char *message) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "error", message);
  char *body = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  const esp_err_t err = httpd_resp_sendstr(req, body ? body : "{}");
  free(body);
  return err;
}

// Reads the request body into `buf` (NUL-terminated). Returns nullptr when the
// body is missing or too large.
static const char *read_body(httpd_req_t *req, char *buf, size_t buf_size) {
  if (req->content_len == 0 || req->content_len >= buf_size) {
    return nullptr;
  }
  int received = 0;
  while (received < req->content_len) {
    const int ret = httpd_req_recv(req, buf + received,
                                   req->content_len - received);
    if (ret <= 0) {
      return nullptr;
    }
    received += ret;
  }
  buf[received] = '\0';
  return buf;
}

static const char *state_name(AppState state) {
  switch (state) {
  case AppState::SLEEPING:
    return "SLEEPING";
  case AppState::SETUP:
    return "SETUP";
  case AppState::OPERATIONAL:
    return "OPERATIONAL";
  case AppState::ERROR:
    return "ERROR";
  case AppState::DEGRADED:
    return "DEGRADED";
  }
  return "UNKNOWN";
}

// --- GET / -------------------------------------------------------------------

static esp_err_t root_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, CONFIG_PAGE_HTML, HTTPD_RESP_USE_STRLEN);
}

// --- GET /api/info -----------------------------------------------------------

static esp_err_t info_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "app", "obegransad");
  cJSON_AddStringToObject(root, "idf", esp_get_idf_version());

  cJSON *scenes = cJSON_AddArrayToObject(root, "scenes");
  for (int i = 0; i < scene_count(); i++) {
    const char *name = scene_name_at(i);
    cJSON_AddItemToArray(scenes, cJSON_CreateString(name ? name : ""));
  }

  cJSON *presets_json = cJSON_AddArrayToObject(root, "presets");
  for (uint8_t p = 0; p < PRESET_COUNT; p++) {
    cJSON *preset = cJSON_CreateObject();
    cJSON_AddNumberToObject(preset, "index", p);
    const char *name = preset_name_at(p);
    cJSON_AddStringToObject(preset, "name", name ? name : "");
    cJSON_AddNumberToObject(preset, "dwell_ms", presets[p].dwell_ms);
    cJSON *preset_scenes = cJSON_AddArrayToObject(preset, "scenes");
    for (uint8_t i = 0; i < preset_scene_count_at(p); i++) {
      const char *scene_name = preset_scene_name_at(p, i);
      cJSON_AddItemToArray(preset_scenes,
                           cJSON_CreateString(scene_name ? scene_name : ""));
    }
    cJSON_AddItemToArray(presets_json, preset);
  }
  return send_json(req, root);
}

// --- GET /api/state ----------------------------------------------------------

static esp_err_t state_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "state",
                          state_name(StateMachine::instance().get_state()));
  cJSON_AddBoolToObject(root, "wifi", wifi_check());
  const uint8_t preset = preset_current();
  cJSON_AddNumberToObject(root, "preset", preset);
  const char *preset_name = preset_name_at(preset);
  cJSON_AddStringToObject(root, "preset_name",
                          preset_name ? preset_name : "");
  cJSON_AddStringToObject(root, "scene", current_scene_name());
  cJSON_AddNumberToObject(root, "brightness", panel_get_global_brightness());
  cJSON_AddNumberToObject(root, "uptime_ms", (double)millis());

  struct tm timeinfo = {};
  if (get_local_time(timeinfo)) {
    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
    cJSON_AddStringToObject(root, "time", buf);
  } else {
    cJSON_AddNullToObject(root, "time");
  }

  cJSON *weather = cJSON_AddObjectToObject(root, "weather");
  const bool valid = weather_is_valid();
  cJSON_AddBoolToObject(weather, "valid", valid);
  if (valid) {
    const WeatherData data = weather_get();
    cJSON_AddNumberToObject(weather, "temperature", data.temperature);
    cJSON_AddNumberToObject(weather, "code", data.weatherCode);
    cJSON_AddBoolToObject(weather, "is_day", data.isDay);
  } else {
    cJSON_AddNullToObject(weather, "temperature");
    cJSON_AddNullToObject(weather, "code");
    cJSON_AddNullToObject(weather, "is_day");
  }
  return send_json(req, root);
}

// --- GET/POST /api/config ----------------------------------------------------

static void config_to_json(cJSON *root) {
  cJSON_AddStringToObject(root, "timezone", config_get_timezone());
  cJSON_AddNumberToObject(root, "latitude", config_get_latitude());
  cJSON_AddNumberToObject(root, "longitude", config_get_longitude());
  cJSON_AddNumberToObject(root, "anniversary_day",
                          config_get_anniversary_day());
  cJSON_AddNumberToObject(root, "anniversary_month",
                          config_get_anniversary_month());
  cJSON_AddNumberToObject(root, "brightness", config_get_brightness());
}

static esp_err_t config_get_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  config_to_json(root);
  return send_json(req, root);
}

static esp_err_t config_post_handler(httpd_req_t *req) {
  char buf[512];
  const char *body = read_body(req, buf, sizeof(buf));
  if (body == nullptr) {
    return send_error(req, "400 Bad Request", "missing or oversized body");
  }
  cJSON *root = cJSON_Parse(body);
  if (root == nullptr) {
    return send_error(req, "400 Bad Request", "invalid JSON");
  }

  esp_err_t err = ESP_OK;
  const cJSON *item = nullptr;
  if ((item = cJSON_GetObjectItem(root, "timezone")) != nullptr) {
    if (!cJSON_IsString(item) ||
        config_set_timezone(item->valuestring) != ESP_OK) {
      err = ESP_ERR_INVALID_ARG;
    }
  }
  const cJSON *lat = cJSON_GetObjectItem(root, "latitude");
  const cJSON *lon = cJSON_GetObjectItem(root, "longitude");
  if (err == ESP_OK && (lat != nullptr || lon != nullptr)) {
    if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon) ||
        config_set_location(lat->valuedouble, lon->valuedouble) != ESP_OK) {
      err = ESP_ERR_INVALID_ARG;
    }
  }
  const cJSON *day = cJSON_GetObjectItem(root, "anniversary_day");
  const cJSON *month = cJSON_GetObjectItem(root, "anniversary_month");
  if (err == ESP_OK && (day != nullptr || month != nullptr)) {
    if (!cJSON_IsNumber(day) || !cJSON_IsNumber(month) ||
        config_set_anniversary(day->valueint, month->valueint) != ESP_OK) {
      err = ESP_ERR_INVALID_ARG;
    }
  }
  if ((item = cJSON_GetObjectItem(root, "brightness")) != nullptr) {
    if (!cJSON_IsNumber(item) || item->valueint < 0 ||
        item->valueint > 255 ||
        config_set_brightness((uint8_t)item->valueint) != ESP_OK) {
      err = ESP_ERR_INVALID_ARG;
    }
  }
  cJSON_Delete(root);

  if (err != ESP_OK) {
    return send_error(req, "400 Bad Request", "invalid value");
  }
  cJSON *result = cJSON_CreateObject();
  config_to_json(result);
  return send_json(req, result);
}

// --- POST /api/control -------------------------------------------------------

static bool map_control(const char *action, const cJSON *value,
                        app_control_cmd_t *cmd, int *arg, const char **str) {
  *arg = 0;
  *str = nullptr;
  if (strcmp(action, "next_preset") == 0) {
    *cmd = APP_CTRL_NEXT_PRESET;
  } else if (strcmp(action, "prev_preset") == 0) {
    *cmd = APP_CTRL_PREV_PRESET;
  } else if (strcmp(action, "refresh_weather") == 0) {
    *cmd = APP_CTRL_REFRESH_WEATHER;
  } else if (strcmp(action, "redraw") == 0) {
    *cmd = APP_CTRL_REDRAW;
  } else if (strcmp(action, "preset") == 0 && cJSON_IsNumber(value)) {
    *cmd = APP_CTRL_PRESET;
    *arg = value->valueint;
  } else if (strcmp(action, "brightness") == 0 && cJSON_IsNumber(value)) {
    *cmd = APP_CTRL_BRIGHTNESS;
    *arg = value->valueint;
  } else if (strcmp(action, "scene") == 0 && cJSON_IsString(value)) {
    *cmd = APP_CTRL_SCENE;
    *str = value->valuestring;
  } else if (strcmp(action, "fake_weather") == 0 && cJSON_IsString(value)) {
    *cmd = APP_CTRL_FAKE_WEATHER;
    *str = value->valuestring;
  } else if (strcmp(action, "fake_time") == 0) {
    *cmd = APP_CTRL_FAKE_TIME;
    if (cJSON_IsString(value)) {
      *str = value->valuestring;
    } else if (value == nullptr || cJSON_IsNull(value)) {
      *str = ""; // clear the override
    } else {
      return false;
    }
  } else {
    return false;
  }
  return true;
}

static esp_err_t control_handler(httpd_req_t *req) {
  char buf[512];
  const char *body = read_body(req, buf, sizeof(buf));
  if (body == nullptr) {
    return send_error(req, "400 Bad Request", "missing or oversized body");
  }
  cJSON *root = cJSON_Parse(body);
  if (root == nullptr) {
    return send_error(req, "400 Bad Request", "invalid JSON");
  }
  const cJSON *action = cJSON_GetObjectItem(root, "action");
  if (!cJSON_IsString(action)) {
    cJSON_Delete(root);
    return send_error(req, "400 Bad Request", "missing action");
  }
  app_control_cmd_t cmd = APP_CTRL_REDRAW;
  int arg = 0;
  const char *str = nullptr;
  const bool mapped = map_control(action->valuestring,
                                  cJSON_GetObjectItem(root, "value"), &cmd,
                                  &arg, &str);
  // `str` points into the JSON tree; copy it before deleting the tree.
  char value_copy[32] = {};
  const bool has_value = str != nullptr;
  if (has_value) {
    strlcpy(value_copy, str, sizeof(value_copy));
  }
  cJSON_Delete(root);
  if (!mapped) {
    return send_error(req, "400 Bad Request", "unknown action or value");
  }

  const esp_err_t err =
      app_control_request(cmd, arg, has_value ? value_copy : nullptr);
  if (err == ESP_ERR_NOT_SUPPORTED) {
    return send_error(req, "400 Bad Request",
                      "action only available in simulator builds");
  }
  if (err != ESP_OK) {
    return send_error(req, "503 Service Unavailable", "control queue full");
  }
  cJSON *result = cJSON_CreateObject();
  cJSON_AddBoolToObject(result, "queued", true);
  httpd_resp_set_status(req, "202 Accepted");
  return send_json(req, result);
}

// --- GET /api/events ---------------------------------------------------------

static esp_err_t events_handler(httpd_req_t *req) {
  uint32_t since = 0;
  char query[64];
  char value[16];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
      httpd_query_key_value(query, "since", value, sizeof(value)) == ESP_OK) {
    since = (uint32_t)strtoul(value, nullptr, 10);
  }

  app_event_record_t records[APP_EVENT_HISTORY];
  uint32_t latest = 0;
  const size_t count =
      app_events_since(since, records, APP_EVENT_HISTORY, &latest);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "latest", latest);
  cJSON *events = cJSON_AddArrayToObject(root, "events");
  for (size_t i = 0; i < count; i++) {
    cJSON *event = cJSON_CreateObject();
    cJSON_AddNumberToObject(event, "seq", records[i].seq);
    cJSON_AddStringToObject(event, "id", app_event_name(records[i].id));
    cJSON_AddNumberToObject(event, "uptime_ms", records[i].uptime_ms);
    cJSON_AddItemToArray(events, event);
  }
  return send_json(req, root);
}

// --- server setup ------------------------------------------------------------

esp_err_t config_server_start(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = CONFIG_OBG_HTTP_PORT;
  config.max_uri_handlers = 12;
  config.lru_purge_enable = true;
  config.stack_size = 6144;

  httpd_handle_t server = nullptr;
  esp_err_t err = httpd_start(&server, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
    return err;
  }

  static const struct {
    const char *uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t *);
  } entries[] = {
      {"/", HTTP_GET, root_handler},
      {"/api/info", HTTP_GET, info_handler},
      {"/api/state", HTTP_GET, state_handler},
      {"/api/config", HTTP_GET, config_get_handler},
      {"/api/config", HTTP_POST, config_post_handler},
      {"/api/control", HTTP_POST, control_handler},
      {"/api/events", HTTP_GET, events_handler},
  };
  for (const auto &entry : entries) {
    httpd_uri_t uri = {};
    uri.uri = entry.uri;
    uri.method = entry.method;
    uri.handler = entry.handler;
    err = httpd_register_uri_handler(server, &uri);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "registering %s failed: %s", entry.uri,
               esp_err_to_name(err));
      return err;
    }
  }

  ESP_LOGI(TAG, "config server listening on port %d (see docs/openapi.yaml)",
           CONFIG_OBG_HTTP_PORT);
  return ESP_OK;
}
