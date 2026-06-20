#include "esp_err.h"
#include "esp_log.h"

#define ESP_RETURN_ON_ERROR_WITH_NAME(x, tag, format, ...)                     \
  do {                                                                         \
    esp_err_t err_rc_ = (x);                                                   \
    if (unlikely(err_rc_ != ESP_OK)) {                                         \
      ESP_LOGE(tag, format " (error: %s)", ##__VA_ARGS__,                      \
               esp_err_to_name(err_rc_));                                      \
      return err_rc_;                                                          \
    }                                                                          \
  } while (0)