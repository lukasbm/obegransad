#include <time.h>

// TODO: move to kconfig!
#define NTP_SERVER "pool.ntp.org"

void clock_init(const char *tz);
bool get_local_time(struct tm &timeinfo);
bool get_time_precise(struct timeval &tv);
time_t get_unix_time(void);
void clock_force_sync();

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
