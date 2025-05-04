#pragma once

#include <Arduino.h>

// time
void time_setup();
void time_syncNTP();

// get time
static void time_fetch();
int time_hour();
int time_minute();
int time_second();
bool isNight();
const char *getTimeString();
struct tm time_full();
