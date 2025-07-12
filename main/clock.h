// TODO: move to kconfig!
#define MY_NTP_SERVER "pool.ntp.org"

void time_syncNTP();

bool shouldTurnOff(struct tm const &time);
time_t calcTurnOffDuration(struct tm time);

// gets local time (from RTC) and updates if needed
struct tm time_get();

enum MoonPhase : uint8_t {
  NEW,
  WAXING_CRESCENT,
  FIRST_QUARTER,
  WAXING_GIBBOUS,
  FULL,
  WANING_GIBBOUS,
  LAST_QUARTER,
  WANING_CRESCENT,
};

MoonPhase calcMoonPhase(struct tm const &time);
