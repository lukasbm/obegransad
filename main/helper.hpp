#pragma once

#include <esp_timer.h>
#include <functional>
#include <stdint.h>

// Milliseconds since boot (Arduino-style helper used by scenes).
inline unsigned long millis() {
  return static_cast<unsigned long>(esp_timer_get_time() / 1000);
}

// pos is one of the 60 corner pixels. writes the x and y coordinates.
// pos 0 is top left, moving clockwise
inline void ring_coord(uint8_t pos, uint8_t &x, uint8_t &y) {
  if (pos < 16) // 16 in the top row
  {
    x = pos;
    y = 0;
  } else if (pos < 30) // 16 in the top row and 14 down
  {
    x = 15;
    y = pos - 15;
  } else if (pos < 46) {
    x = 45 - pos;
    y = 15;
  } else if (pos < 60) {
    x = 0;
    y = 60 - pos;
  }
}

/**
 * @brief Timer class for rendering tasks
 * This class uses ESP-IDF's esp_timer to create a periodic timer that can be
 * used to trigger rendering updates at a specified interval.
 * The user can then poll it in their rendering loop to check if the interval
 * has passed.
 */
struct RenderTimer {

private:
  uint32_t interval_us;
  volatile bool ticked = false;
  esp_timer_handle_t timer = nullptr;
  std::function<void()> user_callback;

  static void timer_callback(void *arg) {
    RenderTimer *instance = static_cast<RenderTimer *>(arg);
    instance->ticked = true;
    if (instance->user_callback) {
      instance->user_callback();
    }
  }

public:
  /**
   * @brief Constructor without user callback
   * user has to poll the check() method to see if the timer has ticked
   */
  RenderTimer(const char *name, uint32_t interval_ms)
      : RenderTimer(name, interval_ms, nullptr) {}

  /**
   * @brief Constructor with user callback
   * The user can provide a callback function that will be called when the
   * timer ticks.
   * The callback will be called in the context of the timer, so it should be
   * lightweight and not block for long periods.
   * @note the check function still works
   */
  RenderTimer(const char *name, uint32_t interval_ms,
              std::function<void()> callback)
      : interval_us(interval_ms * 1000), user_callback(callback) {
    esp_timer_create_args_t timer_args = {
        .callback = &RenderTimer::timer_callback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = name,
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &timer);
  }

  ~RenderTimer() {
    if (timer) {
      stop();
      esp_timer_delete(timer);
    }
  }

  void start() {
    if (timer) {
      esp_timer_start_periodic(timer, interval_us);
    }

    // immediately trigger the callback once
    // timer_callback(this);
  }

  void stop() {
    if (timer) {
      esp_timer_stop(timer);
    }
  }

  bool check() {
    if (ticked) {
      ticked = false; // reset the ticked flag
      return true;    // interval has passed
    } else {
      return false; // interval has not passed
    }
  }
};
