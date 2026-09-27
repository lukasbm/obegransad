// Minimal esp_timer implementation for the Linux/POSIX host target, where
// ESP-IDF ships the esp_timer headers without an implementation. Compiled to
// an empty translation unit on chip targets (the real esp_timer is linked
// there).
//
// Semantics are close enough for the simulator: timers are backed by a
// FreeRTOS task that sleeps for the period and invokes the callback in task
// context. ESP_TIMER_ISR dispatch cannot be emulated (there are no ISRs on the
// host), which is why panel_core.c notifies its task differently on Linux.

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_LINUX

#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// esp_timer_get_time() is microseconds since boot; on the host that is
// microseconds since this process started.
static int64_t s_start_us;
static void __attribute__((constructor)) shim_record_start(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  s_start_us = (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

int64_t esp_timer_get_time(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000 - s_start_us;
}

struct esp_timer {
  esp_timer_cb_t callback;
  void *arg;
  char name[24];
  TaskHandle_t task;
  volatile bool running;
  uint64_t period_us;
  bool periodic;
};

static void timer_task(void *arg) {
  struct esp_timer *t = arg;
  while (t->running) {
    // Round up to at least one tick: with CONFIG_FREERTOS_HZ=100 a 2ms period
    // would otherwise become vTaskDelay(0), i.e. a busy loop that starves
    // lower-priority tasks. Host timing is approximate by design.
    TickType_t ticks = pdMS_TO_TICKS(t->period_us / 1000);
    vTaskDelay(ticks > 0 ? ticks : 1);
    if (!t->running) {
      break;
    }
    t->callback(t->arg);
    if (!t->periodic) {
      t->running = false;
    }
  }
  t->task = NULL;
  vTaskDelete(NULL);
}

esp_err_t esp_timer_create(const esp_timer_create_args_t *args,
                           esp_timer_handle_t *out) {
  if (args == NULL || out == NULL || args->callback == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  struct esp_timer *t = calloc(1, sizeof(*t));
  if (t == NULL) {
    return ESP_ERR_NO_MEM;
  }
  t->callback = args->callback;
  t->arg = args->arg;
  if (args->name) {
    strncpy(t->name, args->name, sizeof(t->name) - 1);
  }
  *out = t;
  return ESP_OK;
}

static esp_err_t timer_start(struct esp_timer *t, uint64_t period_us,
                             bool periodic) {
  if (t == NULL || period_us == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  if (t->task != NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  t->period_us = period_us;
  t->periodic = periodic;
  t->running = true;
  if (xTaskCreate(timer_task, t->name[0] ? t->name : "esp_timer", 3072, t, 5,
                  &t->task) != pdPASS) {
    t->running = false;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t period_us) {
  return timer_start(t, period_us, true);
}

esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t timeout_us) {
  return timer_start(t, timeout_us, false);
}

esp_err_t esp_timer_stop(esp_timer_handle_t t) {
  if (t == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  t->running = false;
  return ESP_OK;
}

esp_err_t esp_timer_delete(esp_timer_handle_t t) {
  if (t == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  t->running = false;
  // Let the timer task notice and exit before freeing its argument.
  for (int i = 0; i < 100 && t->task != NULL; i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  free(t);
  return ESP_OK;
}

bool esp_timer_is_active(esp_timer_handle_t t) {
  return t != NULL && t->running;
}

#endif // CONFIG_IDF_TARGET_LINUX
