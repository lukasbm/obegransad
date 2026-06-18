#include "clock.h"

#include "app_events.h"
#include "esp_log.h"
#include <esp_netif_sntp.h>
#include <esp_sntp.h>
#include <esp_timer.h>
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

static const char *TAG = "clock";

constexpr time_t MIN_VALID_TIME = 1577836800; // 2020-01-01 00:00:00 UTC

// Microsecond boot-relative timestamp of the last successful NTP sync (0 = never).
static volatile int64_t last_sync_us = 0;
static bool sntp_initialized = false;

// Runs in the SNTP service task context — keep it minimal: record the sync and
// announce it on the app event bus. No panel / blocking work here.
static void on_sntp_synced(struct timeval *tv) {
  last_sync_us = esp_timer_get_time();
  ESP_LOGI(TAG, "NTP time synchronized");
  app_post_event(APP_EVT_TIME_SYNCED);
}

esp_err_t clock_init(const char *tz) {
  // for timezones see
  // Explanation
  // https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html Db:
  // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv or
  // zones.csv in this repo
  clock_apply_timezone(tz);

  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
  config.server_from_dhcp = false;
  config.start = true;          // begin background polling immediately
  config.smooth_sync = false;
  config.wait_for_sync = false; // never block boot waiting for the network
  config.sync_cb = on_sntp_synced;

  esp_err_t ret = esp_netif_sntp_init(&config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SNTP: %s", esp_err_to_name(ret));
    return ret;
  }
  sntp_initialized = true;
  return ESP_OK;
}

void clock_apply_timezone(const char *tz) {
  setenv("TZ", tz, 1);
  tzset();
  ESP_LOGI(TAG, "Timezone applied: %s", tz);
}

void clock_start_sync() {
  if (!sntp_initialized) {
    return;
  }
  // Force an immediate poll (e.g. right after Wi-Fi connects) instead of
  // waiting for the next scheduled SNTP interval.
  esp_sntp_restart();
}

bool is_time_sync_healthy() {
  if (last_sync_us == 0) {
    return false; // never synced
  }
  if (time(NULL) < MIN_VALID_TIME) {
    return false; // clock not plausibly set
  }
  // Consider sync stale after 24h (lwIP auto-resyncs hourly, so this is slack).
  const int64_t age_us = esp_timer_get_time() - last_sync_us;
  return age_us < (24LL * 3600 * 1000000LL);
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

  // Check if time is reasonable (after 2020). This is a per-frame query for the
  // clock scenes, so don't log here — the bool return is the signal.
  if (now < MIN_VALID_TIME) { // 2020-01-01 00:00:00 UTC
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

struct tm time_get() {
  struct tm timeinfo = {};
  get_local_time(timeinfo);
  return timeinfo;
}
