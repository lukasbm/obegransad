#include "clock.h"
#include "config.h"

void time_syncNTP()
{
    configTzTime(gSettings.timezone.c_str(), MY_NTP_SERVER);
    struct tm temp;
    // check if we can get the local time
    if (!getLocalTime(&temp, 2000))
    {
        Serial.println("Failed to obtain time");
        return;
    }
}

// common night time hours, only used if no wifi is present
bool time_isNight(struct tm const &time)
{
    if (time.tm_hour >= 22 || time.tm_hour < 6)
    {
        return true; // night
    }
    return false; // day
}

bool shouldTurnOff(struct tm const &time)
{
    return gSettings.off_hours & (1 << time.tm_hour);
}

time_t calcTurnOffDuration(struct tm time)
{
    // calculate number of hours until the next on hour
    uint8_t i = time.tm_hour;
    while (i < 24 && !(gSettings.off_hours & (1 << i)))
    {
        i++;
    }
    time_t offDuration = (time.tm_hour - i) * 3600; // number of seconds in the off window
    // duplicate time and round it down to the nearest hour
    struct tm roundedTime = time;
    roundedTime.tm_min = 0;
    roundedTime.tm_sec = 0;
    // calc diff
    time_t overhead = difftime(mktime(&time), mktime(&roundedTime));
    return offDuration - overhead; // return the number of seconds until the next on hour
}

struct tm time_get()
{
    struct tm timeinfo;
    getLocalTime(&timeinfo, 200);
    return timeinfo;
}

// https://fcds.cs.put.poznan.pl/MyWeb/Praca/Ubiquitous/LunarPhases.pdf
MoonPhase getMoonPhase(struct tm &time)
{
    return FULL; // TODO: implement this properly for moon calendar scene?
}
