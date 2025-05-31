#pragma once

#include <Arduino.h>

#define MY_NTP_SERVER "pool.ntp.org"

void time_setup();

void time_syncNTP();

bool isNight(struct tm const &time);

bool shouldTurnOff(struct tm const &time);

// gets local time and updates if needed
struct tm time_fetch();

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
