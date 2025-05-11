#pragma once

#include <Arduino.h>

#define MY_NTP_SERVER "pool.ntp.org"

void time_setup();

void time_syncNTP();

bool isNight(struct tm time);

bool shouldTurnOff(struct tm time);

// gets local time and updates if needed
struct tm time_fetch();
