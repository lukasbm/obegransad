#include "app_control.h"

#include "app_events.h"
#include "clock.h"
#include "config_store.h"
#include "ikea-obegransad-panel.h"
#include "scene_switcher.h"
#include "sdkconfig.h"
#include "state.h"
#include "weather.h"
#include "weather_client.h"

#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

static const char *TAG = "control";

static constexpr int QUEUE_LEN = 8;
static constexpr size_t STR_LEN = 32;

struct control_request_t {
  app_control_cmd_t cmd;
  int arg;
  char str[STR_LEN];
};

static QueueHandle_t s_queue;

esp_err_t app_control_init(void) {
  if (s_queue == nullptr) {
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(control_request_t));
  }
  return s_queue != nullptr ? ESP_OK : ESP_ERR_NO_MEM;
}

static bool is_sim_only(app_control_cmd_t cmd) {
  return cmd == APP_CTRL_FAKE_WEATHER || cmd == APP_CTRL_FAKE_TIME;
}

esp_err_t app_control_request(app_control_cmd_t cmd, int arg, const char *str) {
  if (s_queue == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
#if !CONFIG_OBG_SIMULATOR
  if (is_sim_only(cmd)) {
    return ESP_ERR_NOT_SUPPORTED;
  }
#endif

  control_request_t req = {};
  req.cmd = cmd;
  req.arg = arg;
  if (str != nullptr) {
    strlcpy(req.str, str, sizeof(req.str));
  }
  if (xQueueSend(s_queue, &req, 0) != pdTRUE) {
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

// Builds a plausible WeatherData snapshot for a named condition so scenes can
// be exercised without a network fetch.
static WeatherData fake_weather(const char *condition) {
  WeatherData data = {};
  data.requestTime = get_unix_timestamp();
  data.isDay = true;

  if (strcmp(condition, "clear") == 0) {
    data.weatherCode = WEATHER_CLEAR;
    data.temperature = 22.0f;
  } else if (strcmp(condition, "cloudy") == 0) {
    data.weatherCode = WEATHER_CLOUDY;
    data.temperature = 17.0f;
  } else if (strcmp(condition, "rain") == 0) {
    data.weatherCode = WEATHER_MODERATE_RAIN;
    data.temperature = 13.0f;
  } else if (strcmp(condition, "snow") == 0) {
    data.weatherCode = WEATHER_MODERATE_SNOW_FALL;
    data.temperature = -2.0f;
  } else {
    data.weatherCode = WEATHER_INVALID; // "invalid": scenes show placeholders
    data.temperature = 0.0f;
  }

  const time_t day = 24 * 3600;
  for (uint8_t i = 0; i < FORECAST_DAYS; i++) {
    data.daily[i].sunrise = data.requestTime + i * day + 7 * 3600;
    data.daily[i].sunset = data.requestTime + i * day + 19 * 3600;
    data.daily[i].uvIndexMax = 4.0f;
    data.daily[i].temperatureMax = data.temperature + 4.0f;
    data.daily[i].temperatureMin = data.temperature - 5.0f;
    data.daily[i].temperatureMean = data.temperature;
    data.daily[i].weatherCode = data.weatherCode;
  }
  return data;
}

static void execute(const control_request_t &req) {
  switch (req.cmd) {
  case APP_CTRL_NEXT_PRESET:
    preset_next();
    StateMachine::instance().announce_preset();
    break;
  case APP_CTRL_PREV_PRESET:
    preset_prev();
    StateMachine::instance().announce_preset();
    break;
  case APP_CTRL_PRESET:
    if (req.arg >= 0 && req.arg < 10) {
      preset_select((uint8_t)req.arg);
      StateMachine::instance().announce_preset();
    }
    break;
  case APP_CTRL_SCENE:
    if (!scene_select_by_name(req.str)) {
      ESP_LOGW(TAG, "unknown scene '%s'", req.str);
    }
    break;
  case APP_CTRL_BRIGHTNESS:
    if (req.arg >= 0 && req.arg <= 255) {
      config_set_brightness((uint8_t)req.arg);
    }
    break;
  case APP_CTRL_REFRESH_WEATHER:
    weather_client_request_fetch();
    break;
  case APP_CTRL_REDRAW:
    scene_force_redraw();
    panel_commit();
    break;
  case APP_CTRL_FAKE_WEATHER:
#if CONFIG_OBG_SIMULATOR
    weather_set(fake_weather(req.str));
    app_post_event(APP_EVT_WEATHER_DATA_READY);
#endif
    break;
  case APP_CTRL_FAKE_TIME:
#if CONFIG_OBG_SIMULATOR
    if (req.str[0] == '\0') {
      clock_sim_set_time(-1, -1);
    } else {
      int hour = 0;
      int minute = 0;
      if (sscanf(req.str, "%d:%d", &hour, &minute) == 2) {
        clock_sim_set_time(hour, minute);
      }
    }
#endif
    break;
  }
}

void app_control_process(void) {
  if (s_queue == nullptr) {
    return;
  }
  control_request_t req;
  while (xQueueReceive(s_queue, &req, 0) == pdTRUE) {
    execute(req);
  }
}
