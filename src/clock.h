#pragma once

#include <Arduino.h>

#define MY_NTP_SERVER "pool.ntp.org"

void time_syncNTP();

bool time_isNight(struct tm const &time);

bool shouldTurnOff(struct tm const &time);

// gets local time (from RTC) and updates if needed
struct tm time_get();

enum MoonPhase : uint8_t
{
    NEW,
    WAXING_CRESCENT,
    FIRST_QUARTER,
    WAXING_GIBBOUS,
    FULL,
    WANING_GIBBOUS,
    LAST_QUARTER,
    WANING_CRESCENT,
};

MoonPhase calculateMoonPhase(struct tm const &time);
