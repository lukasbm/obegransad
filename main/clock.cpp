#include "clock.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <esp_netif_sntp.h>
#include <sys/_intsup.h>
#include <sys/select.h>
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
  esp_err_t ret = esp_netif_sntp_init(&config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SNTP: %s", esp_err_to_name(ret));
    return;
  }

  // Wait for initial sync with timeout
  if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
    ESP_LOGW(
        TAG,
        "Failed to sync time within 10s timeout - will sync in background");
  } else {
    ESP_LOGI(TAG, "Time synchronized successfully");
  }
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

