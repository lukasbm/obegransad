#include "clock.h"

#include "esp_log.h"
#include <esp_netif_sntp.h>
#include <sys/_intsup.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

/*
functions of standard C api (POSIX)):
- gettimeofday: for microsecond resolution (UTC based)
- time: preferred for second resolution (UTC based)
- localtime: converts time_t to struct tm in local timezone
- localtime_r: thread-safe version of localtime
- settimeofday: sets time from struct timeval (unsmoothed)
- asctime: converts struct tm to string in local timezone
- gmtime: converts time_t to struct tm in UTC
- mktime: converts struct tm to time_t in local timezone
- strftime: formats struct tm to string
- strptime: parses string to struct tm
- tzset: initializes timezone information from TZ environment variable
- clock: returns processor time used by the program
- ctime: converts time_t to string
- difftime: calculates difference between two time_t values
- adjtime: adjusts system time by a specified amount (smoothes time
  adjustments)
*/

const char *TAG = "clock";

constexpr time_t MIN_VALID_TIME = 1577836800; // 2020-01-01 00:00:00 UTC

void clock_init(const char *tz) {
  // for timezones see
  // Explanation
  // https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html Db:
  // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv or
  // zones.csv in this repo
  setenv("TZ", tz, 1);
  tzset();

  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
  config.server_from_dhcp = false;
  config.start = true;
  config.smooth_sync = false;
  config.wait_for_sync = true;

  esp_err_t ret = esp_netif_sntp_init(&config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SNTP: %s", esp_err_to_name(ret));
    return;
  }

  // Wait for initial sync with timeout
  clock_force_sync();
}

// TODO: implement!
// bool is_time_sync_healthy() {
//     // Check if we have a reasonable time (after 2020)
//     time_t now = time(NULL);
//     if (now < 1577836800) {  // 2020-01-01 00:00:00 UTC
//         return false;
//     }

//     // Check if sync is too old (more than 24 hours)
//     int64_t now_us = esp_timer_get_time();
//     if (last_sync_time > 0 && (now_us - last_sync_time) > (24 * 3600 *
//     1000000LL)) {
//         ESP_LOGW(TAG, "Time sync is stale (>24h old)");
//         return false;
//     }

//     return true;
// }

void clock_force_sync() {
  static bool time_sync_in_progress = false;
  if (time_sync_in_progress) {
    ESP_LOGD(TAG, "Sync already in progress, skipping");
    return;
  }

  time_sync_in_progress = true;
  ESP_LOGI(TAG, "Forcing immediate NTP sync");

  esp_err_t ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(5000));
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Force sync successful");
  } else {
    ESP_LOGW(TAG, "Force sync failed: %s", esp_err_to_name(ret));
  }

  time_sync_in_progress = false;
}

bool get_local_time(struct tm &timeinfo) {
  time_t now;
  time(&now);

  // Check if time is reasonable (after 2020)
  if (now < MIN_VALID_TIME) { // 2020-01-01 00:00:00 UTC
    ESP_LOGW(TAG, "Time not synchronized yet");
    return false;
  }

  localtime_r(&now, &timeinfo);
  return true;
}

// Note: this is in UTC!
bool get_time_precise(struct timeval &tv) {
  if (gettimeofday(&tv, NULL) != 0) {
    ESP_LOGE(TAG, "gettimeofday failed");
    return false;
  }

  // Check if time is reasonable
  if (tv.tv_sec < MIN_VALID_TIME) {
    ESP_LOGW(TAG, "Time not synchronized yet");
    return false;
  }

  return true;
}

time_t get_unix_timestamp(void) { return time(NULL); }
