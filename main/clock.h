#pragma once

#include "esp_err.h"
#include <time.h>

// TODO: move to kconfig!
#define NTP_SERVER "pool.ntp.org"

esp_err_t clock_init(const char *tz);
bool get_local_time(struct tm &timeinfo);
bool get_time_precise(struct timeval &tv);
time_t get_unix_time(void);
void clock_force_sync();

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
