#pragma once

#include "esp_err.h"
#include <time.h>

// TODO: move to kconfig!
#define NTP_SERVER "pool.ntp.org"

esp_err_t clock_init(const char *tz);

// Apply a timezone at runtime (e.g. after a config change). Safe to call any time.
void clock_apply_timezone(const char *tz);

bool get_local_time(struct tm &timeinfo);
bool get_time_precise(struct timeval &tv);
time_t get_unix_timestamp(void);
void clock_force_sync();

// Force an immediate NTP re-poll (e.g. right after Wi-Fi connects). Non-blocking.
void clock_start_sync();

// True if the clock has synced at least once and the sync is not stale (>24h).
bool is_time_sync_healthy();

// Convenience accessor for scenes: returns the current local time, or a
// zero-initialized tm if the clock has not synced yet.
struct tm time_get();

// enum MoonPhase : uint8_t {
//   NEW,
//   WAXING_CRESCENT,
//   FIRST_QUARTER,
//   WAXING_GIBBOUS,
//   FULL,
//   WANING_GIBBOUS,
//   LAST_QUARTER,
//   WANING_CRESCENT,
// };

// MoonPhase calcMoonPhase(struct tm const &time);
